// settings.cpp: the rules for the settings. See settings.h for what each function promises.
//
// This file deliberately has no Arduino code in it, not even #include <Arduino.h>. That's
// what lets scripts/test-host.sh compile it with your computer's compiler and test it.

#include "settings.h"

// ctype.h for isspace(), math.h for roundf() and isfinite(), stdlib.h for strtof(), and
// string.h for memcpy()
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// The longest number we'll accept as text. "-10.0" is 5, so 15 is plenty.
#define NUMBER_TEXT_MAX 15

// Keeps a value between low and high: below low gives low, above high gives high, and
// anything in between comes back unchanged
static float clampf(float value, float low, float high)
{
    if (value < low) {
        return low;
    }

    if (value > high) {
        return high;
    }

    return value;
}

// Offsets are kept to one decimal place. That's as fine as the sensor can tell apart, and
// it means the value Home Assistant shows is exactly the value that's saved.
//
// Multiplying by 10, rounding to a whole number, and dividing by 10 again rounds to one
// decimal place: -1.234 becomes -12.34, then -12, then -1.2.
static float roundToTenth(float value)
{
    return roundf(value * 10.0f) / 10.0f;
}

Settings_t defaultSettings(void)
{
    // = {} sets every field to zero first, so nothing is left holding random leftover memory
    Settings_t settings = {};
    settings.intervalMinutes = INTERVAL_DEFAULT_MINUTES;
    settings.temperatureOffset = 0.0f;
    settings.humidityOffset = 0.0f;
    return settings;
}

bool settingsValid(const Settings_t *settings)
{
    // isfinite is false for NaN and infinity, which is what erased or corrupted flash can
    // look like when it's read back as a float.
    //
    // NaN fails every comparison, so without isfinite, NaN >= -10 and NaN <= 10 would both
    // be false, and the range check alone would catch it. isfinite makes that explicit.
    return settings->intervalMinutes >= INTERVAL_MIN_MINUTES
        && settings->intervalMinutes <= INTERVAL_MAX_MINUTES
        && isfinite(settings->temperatureOffset)
        && settings->temperatureOffset >= TEMPERATURE_OFFSET_MIN
        && settings->temperatureOffset <= TEMPERATURE_OFFSET_MAX
        && isfinite(settings->humidityOffset)
        && settings->humidityOffset >= HUMIDITY_OFFSET_MIN
        && settings->humidityOffset <= HUMIDITY_OFFSET_MAX;
}

bool parseNumber(const char *text, size_t length, float *value)
{
    // Empty, or too long to be a sensible number
    if (length == 0 || length > NUMBER_TEXT_MAX) {
        return false;
    }

    // strtof needs a NUL terminated string, and MQTT payloads don't have one, so copy it
    // into a buffer that does. The buffer is one longer than the longest number, to leave
    // room for the NUL.
    char buffer[NUMBER_TEXT_MAX + 1];
    memcpy(buffer, text, length);
    buffer[length] = '\0';

    // strtof ("string to float") sets end to the first character it couldn't use. If
    // that's the start, there was no number at all. It skips spaces at the start by itself.
    char *end = NULL;
    float parsed = strtof(buffer, &end);
    if (end == buffer) {
        return false;
    }

    // Trailing spaces or a newline are fine, as in `echo 5 | mosquitto_pub -s`.
    // Anything else, like "5min", isn't.
    //
    // The (unsigned char) cast is needed because isspace misbehaves on negative values,
    // which is what a plain char above 127 can be.
    while (*end != '\0' && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return false;
    }

    // strtof happily reads "nan" and "inf", and neither is a setting
    if (!isfinite(parsed)) {
        return false;
    }

    *value = parsed;
    return true;
}

bool applySetting(Settings_t *settings, SettingId_t id, float value)
{
    // Each case works the same way: tidy the value up, check whether it differs from
    // what's there, store it, and report whether it changed. The braces around each case
    // give its variables their own scope, so each case can have its own `changed`.
    switch (id) {
        case SETTING_INTERVAL: {
            // Clamp to 1 to 60, then round to a whole minute
            float minutes = roundf(clampf(value, INTERVAL_MIN_MINUTES, INTERVAL_MAX_MINUTES));
            uint16_t interval = (uint16_t)minutes;
            bool changed = interval != settings->intervalMinutes;
            settings->intervalMinutes = interval;
            return changed;
        }
        case SETTING_TEMPERATURE_OFFSET: {
            // Comparing floats with != is usually risky, but it's safe here: both values
            // went through the same roundToTenth, so equal settings are exactly equal
            float offset = roundToTenth(clampf(value, TEMPERATURE_OFFSET_MIN, TEMPERATURE_OFFSET_MAX));
            bool changed = offset != settings->temperatureOffset;
            settings->temperatureOffset = offset;
            return changed;
        }
        case SETTING_HUMIDITY_OFFSET: {
            float offset = roundToTenth(clampf(value, HUMIDITY_OFFSET_MIN, HUMIDITY_OFFSET_MAX));
            bool changed = offset != settings->humidityOffset;
            settings->humidityOffset = offset;
            return changed;
        }
        default:
            // Not a setting, for example SETTING_COUNT. Nothing to do.
            return false;
    }
}

void applyCalibration(const Settings_t *settings, float rawTemperature, float rawHumidity,
                      float *temperature, float *humidity)
{
    // Offsets are added, so a sensor that reads 1.3 °C too warm gets an offset of -1.3
    *temperature = rawTemperature + settings->temperatureOffset;

    // A big positive offset on a humid day could otherwise report 103%
    *humidity = clampf(rawHumidity + settings->humidityOffset, 0.0f, 100.0f);
}

uint64_t sleepMicroseconds(uint16_t intervalMinutes, uint32_t awakeMs)
{
    // The time spent awake counts towards the interval, so readings stay evenly spaced
    // instead of drifting later by a few seconds every time.
    //
    // The math is done in 64 bits. An hour in microseconds is 3.6 billion, which is close
    // to the most a 32 bit number can hold, and any bigger and it would overflow.
    uint64_t intervalMs = (uint64_t)intervalMinutes * 60 * 1000;
    uint64_t sleepMs = MIN_SLEEP_MS;

    // Only subtract when there's enough interval left. Otherwise the unsigned subtraction
    // would go below zero and wrap around to an enormous number.
    if (intervalMs > (uint64_t)awakeMs + MIN_SLEEP_MS) {
        sleepMs = intervalMs - awakeMs;
    }

    // Milliseconds to microseconds
    return sleepMs * 1000;
}

uint32_t expireAfterSeconds(uint16_t intervalMinutes)
{
    // Two intervals in seconds, plus 60 seconds of slack for slow wifi
    return 2 * (uint32_t)intervalMinutes * 60 + 60;
}
