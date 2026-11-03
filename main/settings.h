// settings.h: the things you can change from Home Assistant, and the rules for them.
//
// There are three settings: how often to report, and an offset each for temperature and
// humidity. They arrive as text over MQTT, get checked and tidied up here, and are then
// saved to flash by storage.cpp.
//
// Nothing in here touches the hardware, so it all gets tested on your computer by
// scripts/test-host.sh.

#pragma once

// stddef.h gives us size_t, the type for sizes and lengths.
// stdint.h gives us exact-size number types: uint16_t is a 16 bit unsigned number (0 to
// 65535), uint32_t is 32 bits, and uint64_t is 64 bits. Plain "int" changes size from one
// chip to another, and these don't.
#include <stddef.h>
#include <stdint.h>

// For REPORT_INTERVAL_DEFAULT_MS
#include "config.h"

// How often a reading is sent. Home Assistant shows these limits on its number box.
#define INTERVAL_MIN_MINUTES 1
#define INTERVAL_MAX_MINUTES 60

// The default comes from config.h. A config.h from before it was added gets 5 minutes.
#ifndef REPORT_INTERVAL_DEFAULT_MS
#define REPORT_INTERVAL_DEFAULT_MS 300000
#endif

// config.h has it in milliseconds, but the setting is stored in minutes, since that's
// what Home Assistant shows. There are 60000 ms in a minute.
#define INTERVAL_DEFAULT_MINUTES (REPORT_INTERVAL_DEFAULT_MS / 60000)

// These stop the build with a clear message if the default is something the interval
// setting can't hold, instead of it being quietly rounded.
//
// static_assert is checked by the compiler, not while the firmware runs. If the condition
// is false, the build fails with the message.
static_assert(REPORT_INTERVAL_DEFAULT_MS % 60000 == 0,
              "REPORT_INTERVAL_DEFAULT_MS has to be a whole number of minutes (a multiple of 60000)");
static_assert(INTERVAL_DEFAULT_MINUTES >= INTERVAL_MIN_MINUTES && INTERVAL_DEFAULT_MINUTES <= INTERVAL_MAX_MINUTES,
              "REPORT_INTERVAL_DEFAULT_MS has to be between 1 and 60 minutes (60000 to 3600000)");

// Calibration offsets get added to every reading. Anything bigger than these limits means
// the sensor is broken, not out of calibration.
//
// The f on the end makes these float constants. Without it they'd be double, and mixing
// the two makes the compiler convert back and forth.
#define TEMPERATURE_OFFSET_MIN -10.0f // °C
#define TEMPERATURE_OFFSET_MAX 10.0f
#define HUMIDITY_OFFSET_MIN    -20.0f // Percentage points
#define HUMIDITY_OFFSET_MAX    20.0f

// The shortest sleep, in case a wake ever takes longer than the interval
#define MIN_SLEEP_MS 5000

// The settings themselves. typedef struct ... Settings_t lets the rest of the code write
// Settings_t instead of struct Settings. The _t on the end is a common way of marking a
// type name.
typedef struct Settings {
    uint16_t intervalMinutes;
    float temperatureOffset; // °C
    float humidityOffset;    // Percentage points, so +3 turns 45% into 48%
} Settings_t;

// One per setting. SETTING_COUNT isn't a setting, it's how many there are, which is handy
// for sizing arrays.
//
// An enum gives names to numbers, counting from 0: SETTING_INTERVAL is 0,
// SETTING_TEMPERATURE_OFFSET is 1, and so on, so SETTING_COUNT comes out as 3.
typedef enum SettingId {
    SETTING_INTERVAL,
    SETTING_TEMPERATURE_OFFSET,
    SETTING_HUMIDITY_OFFSET,
    SETTING_COUNT,
} SettingId_t;

// What a device with nothing saved starts with: the interval from config.h, and no correction
Settings_t defaultSettings(void);

// False if anything is out of range or not a number. Used on settings read back from flash.
//
// The const in "const Settings_t *" promises this function only reads the settings.
bool settingsValid(const Settings_t *settings);

// Reads a number out of an MQTT payload like "5" or "-1.2". The payload isn't NUL
// terminated, which is why it comes with a length. False if it isn't a plain, finite number.
//
// The number is written to *value, and only when it returns true.
bool parseNumber(const char *text, size_t length, float *value);

// Sets one setting. Values outside the limits are clamped to the nearest limit, the
// interval is rounded to a whole minute, and the offsets to 0.1. Returns true if the
// setting actually changed.
bool applySetting(Settings_t *settings, SettingId_t id, float value);

// Adds the offsets to a raw reading. Humidity is kept between 0 and 100.
void applyCalibration(const Settings_t *settings, float rawTemperature, float rawHumidity,
                      float *temperature, float *humidity);

// How long to sleep so the next reading lands one interval after this one started.
// It's in microseconds, because that's what ESP.deepSleep() takes.
uint64_t sleepMicroseconds(uint16_t intervalMinutes, uint32_t awakeMs);

// How long Home Assistant should wait for a reading before showing "unavailable".
// Two missed readings plus a minute, so one dropped reading doesn't cause a blip.
uint32_t expireAfterSeconds(uint16_t intervalMinutes);
