// payloads.h: builds everything the device sends to MQTT.
//
// Each function writes JSON into a buffer you give it and returns how many characters it
// wrote. A return of 0 means it didn't fit, or there was nothing valid to send, and the
// buffer shouldn't be published. These have no hardware code in them, so they're tested
// on your computer by scripts/test-host.sh.
//
// JSON is the text format Home Assistant expects: {"name":value, ...}. It's easy for both
// people and programs to read.

#pragma once

#include <stddef.h>
#include <stdint.h>

// For Settings_t
#include "settings.h"

// The buffer sizes for each message, in bytes. Each one is comfortably bigger than the
// message it holds. The tests check that they fit.
#define STATE_PAYLOAD_MAX     96
#define SETTINGS_PAYLOAD_MAX  128
// Discovery is about 2.3 KB with the example names. Longer DEVICE_ID and TOPIC_BASE values
// make it bigger, since they're repeated in every entity, so this leaves plenty of room.
#define DISCOVERY_PAYLOAD_MAX 4096

// The reading, sent every interval:
// {"temperature":21.4,"humidity":45.2,"rssi":-61}
size_t serializeState(float temperature, float humidity, int32_t rssi, char *output, size_t size);

// The current settings, which Home Assistant's number boxes show:
// {"interval":5,"temperature_offset":-1.2,"humidity_offset":3}
size_t serializeSettings(const Settings_t *settings, char *output, size_t size);

// The message that sets the device up in Home Assistant. It depends on the interval,
// because that decides how long Home Assistant waits before calling a reading stale.
size_t serializeDiscovery(uint16_t intervalMinutes, char *output, size_t size);
