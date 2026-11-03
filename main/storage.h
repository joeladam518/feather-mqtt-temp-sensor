// storage.h: what the device remembers between wakes.
//
// The ESP8266 has two places to keep things:
//
//   Flash (EEPROM)  Survives everything, including a power cut. It wears out after many
//                   thousands of writes, so it's only written when a setting changes.
//   RTC memory      A few hundred bytes that stay powered during deep sleep, and are lost
//                   when the power goes. It can be written on every wake for free.
//
// Normal RAM is wiped by deep sleep, so anything not kept in one of these two is gone at
// the next reading.

#pragma once

#include <stdint.h>

// For Settings_t
#include "settings.h"

// What gets kept in RTC memory. The wifi details let the next wake skip scanning for the
// network, which is the slowest part of connecting, and the biggest drain on a battery.
//
// It's 8 bytes: 1 + 6 + 1. RTC memory is read in 4 byte blocks, so if a field is added,
// keep the total a multiple of 4. storage.cpp stops the build if it isn't.
typedef struct RtcState {
    uint8_t wifiChannel;   // 0 means unknown, so scan for it
    uint8_t wifiBssid[6];  // The access point's MAC address
    bool announced;        // Home Assistant has been sent the discovery message
} RtcState_t;

// False if nothing valid was saved, which is the case after a power cut or the first flash.
// settings is left alone then.
bool loadSettings(Settings_t *settings);

// Writes the settings to flash. Only call it when something changed, to spare the flash.
void saveSettings(const Settings_t *settings);

// After a power cut, this gives back a zeroed RtcState_t: no wifi details, and not announced
bool loadRtcState(RtcState_t *state);

// Cheap enough to call on every wake
void saveRtcState(const RtcState_t *state);
