// topics.h: every MQTT topic the device uses, in one place.
//
// A topic is like a file path that messages get sent to, with / separating the levels.
// Keeping them all here means a typo in a topic can only happen once, and the README's
// MQTT table can be checked against a single file.

#pragma once

#include <stddef.h>
#include <string.h>

// For TOPIC_BASE and DEVICE_ID
#include "config.h"

// Two strings written next to each other get joined by the compiler, so with a TOPIC_BASE
// of "temp_sensor", TOPIC_STATE becomes "temp_sensor/state". All of this happens at
// compile time, so there's no string building while the firmware runs.
#define TOPIC_STATE    TOPIC_BASE "/state"    // The readings
#define TOPIC_SETTINGS TOPIC_BASE "/settings" // The current settings, so Home Assistant can show them

// Commands from Home Assistant's number entities. See handleMessage in app.cpp.
#define TOPIC_INTERVAL_SET           TOPIC_BASE "/interval/set"
#define TOPIC_TEMPERATURE_OFFSET_SET TOPIC_BASE "/temperature_offset/set"
#define TOPIC_HUMIDITY_OFFSET_SET    TOPIC_BASE "/humidity_offset/set"

// Decided by Home Assistant, so it doesn't use TOPIC_BASE. Home Assistant listens on
// homeassistant/... for devices announcing themselves.
#define HA_DISCOVERY_TOPIC "homeassistant/device/" DEVICE_ID "/config"

// Incoming topics are not NUL terminated, and comparing the lengths is what stops a topic
// matching one that it's only the start of.
//
// strncmp compares at most `length` characters, so on its own "temp_sensor/state" would
// match "temp_sensor/state/extra". Checking the lengths are equal first rules that out.
//
// "inline" is here because this function is defined in a header, which gets compiled into
// several files.
static inline bool topicEquals(const char *expected, const char *topic, size_t length)
{
    return strlen(expected) == length && strncmp(expected, topic, length) == 0;
}
