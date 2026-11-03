// storage.cpp: saving settings to flash, and the wifi details to RTC memory.
//
// Both kinds of saved data get the same treatment: wrap it in a record with a checksum, and
// only trust what's read back if the checksum still matches. Whatever was in memory before,
// whether that's random bits after a power cut or a different firmware's data, fails the check.
//
// The checksum is crc32(), which comes with the ESP8266 core (it's declared in coredecls.h,
// which Arduino.h includes). A checksum is a number worked out from the data. Change any
// bit of the data and the number comes out different, so a mismatch means the data isn't
// what was saved.

#include "storage.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

#include "logging.h"

// "TMPS" in hex. Data written by some other firmware is very unlikely to start with this.
// It's a quick first check, before the checksum.
#define SETTINGS_MAGIC 0x544D5053

// Bump this when StoredSettings_t changes shape, so the old layout is ignored instead of misread
#define SETTINGS_VERSION 2

// What's written to flash: the settings, wrapped with the checks around them
typedef struct StoredSettings {
    uint32_t magic;
    uint32_t version;
    uint32_t defaultIntervalMinutes; // The default in config.h when these were saved
    Settings_t settings;
    uint32_t crc; // Of everything above it
} StoredSettings_t;

// What's written to RTC memory: the state, with its checksum in front
typedef struct StoredRtcState {
    uint32_t crc; // Of state
    RtcState_t state;
} StoredRtcState_t;

// RTC memory is read and written in whole 4 byte blocks. This stops the build if a change
// to RtcState_t breaks that, instead of it quietly losing the last few bytes.
static_assert(sizeof(StoredRtcState_t) % 4 == 0, "Pad RtcState_t to a multiple of 4 bytes");

// The first 64 bytes of RTC user memory are used by the OTA updater, so stay clear of them.
// The offset is in 4 byte blocks, so 32 means 128 bytes in.
#define RTC_OFFSET_BLOCKS 32

//==============================================================================
// Settings, in flash

// The EEPROM library imitates an Arduino Uno's EEPROM with a block of flash. begin()
// copies it into RAM, get() and put() work on the copy, and commit() writes it back.
//
// The checksum covers every byte up to the crc field. offsetof gives the position of a
// field within a struct, in bytes, which is exactly that length.
static uint32_t settingsCrc(const StoredSettings_t *stored)
{
    return crc32(stored, offsetof(StoredSettings_t, crc));
}

bool loadSettings(Settings_t *settings)
{
    // memset first, so the padding bytes inside the struct have a known value. The
    // checksum covers them too.
    //
    // Padding is unused space the compiler puts between fields to line them up in memory.
    // Its contents would otherwise be whatever was there before.
    StoredSettings_t stored;
    memset(&stored, 0, sizeof(stored));

    // Read the whole record from the start of the EEPROM area
    EEPROM.begin(sizeof(StoredSettings_t));
    EEPROM.get(0, stored);
    EEPROM.end();

    // Never saved, or saved by different firmware or an older layout. Not an error, so no log.
    if (stored.magic != SETTINGS_MAGIC || stored.version != SETTINGS_VERSION) {
        return false;
    }

    // Saved by us, but damaged. Worth a log line.
    if (stored.crc != settingsCrc(&stored) || !settingsValid(&stored.settings)) {
        LOG("STORAGE", "Saved settings are corrupt");
        return false;
    }

    *settings = stored.settings;

    // Flashing doesn't erase saved settings. So without this, an interval saved while testing
    // with a 1 minute default would outlive flashing with a 5 minute one. A new default in
    // config.h is a deliberate change, so it wins. The offsets are kept.
    if (stored.defaultIntervalMinutes != INTERVAL_DEFAULT_MINUTES) {
        LOG("STORAGE", "The default interval changed to %d min, so the interval goes back to it",
            INTERVAL_DEFAULT_MINUTES);
        settings->intervalMinutes = INTERVAL_DEFAULT_MINUTES;

        // Saving records the new default too, so this only happens once per change
        saveSettings(settings);
    }

    return true;
}

void saveSettings(const Settings_t *settings)
{
    // Build the whole record, zeroed first for the same padding reason as above
    StoredSettings_t stored;
    memset(&stored, 0, sizeof(stored));
    stored.magic = SETTINGS_MAGIC;
    stored.version = SETTINGS_VERSION;
    stored.defaultIntervalMinutes = INTERVAL_DEFAULT_MINUTES;
    stored.settings = *settings;

    // The checksum goes last, because it's worked out from everything else
    stored.crc = settingsCrc(&stored);

    // put() only changes the copy in RAM. commit() is the slow part that writes the flash.
    EEPROM.begin(sizeof(StoredSettings_t));
    EEPROM.put(0, stored);
    if (!EEPROM.commit()) {
        LOG("STORAGE", "Saving settings failed");
    }
    EEPROM.end();
}

//==============================================================================
// Wifi details, in RTC memory

bool loadRtcState(RtcState_t *state)
{
    // Start from all zeros, so a failed load leaves a clean state behind
    StoredRtcState_t stored;
    memset(state, 0, sizeof(*state));

    // The library wants a pointer to uint32_t, because it reads in 4 byte blocks.
    // The cast says "treat the struct's memory as a run of those".
    if (!ESP.rtcUserMemoryRead(RTC_OFFSET_BLOCKS, (uint32_t *)&stored, sizeof(stored))) {
        return false;
    }

    // After a power cut, RTC memory holds random bits, and this is where they get caught
    if (stored.crc != crc32(&stored.state, sizeof(stored.state))) {
        return false;
    }

    *state = stored.state;
    return true;
}

void saveRtcState(const RtcState_t *state)
{
    StoredRtcState_t stored;
    memset(&stored, 0, sizeof(stored));
    stored.state = *state;
    stored.crc = crc32(&stored.state, sizeof(stored.state));

    ESP.rtcUserMemoryWrite(RTC_OFFSET_BLOCKS, (uint32_t *)&stored, sizeof(stored));
}
