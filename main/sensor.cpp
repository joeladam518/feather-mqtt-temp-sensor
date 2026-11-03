// sensor.cpp: reads the AM2301 temperature and humidity sensor.
//
// The AM2301 is a DHT22 in a bigger case with wires on it, and Adafruit's DHT library calls
// it a DHT21. It talks over one wire with a timing based protocol: the ESP8266 pulls the line
// low to ask for a reading, then the sensor answers with 40 bits, where the length of each
// high pulse says whether it's a 0 or a 1. The library does all of that.
//
// The 40 bits are 16 for humidity, 16 for temperature, and 8 for a checksum. If the
// checksum doesn't match, the library reports a failed reading instead of a wrong one.

#include "config.h"
#include "sensor.h"

#include <Arduino.h>
#include <DHT.h>
#include <math.h>

#include "logging.h"

// The sensor ignores requests for this long after it gets power
#define SENSOR_STARTUP_MS 2000

// And it needs this long between readings, so a retry has to wait too
#define SENSOR_RETRY_MS 2100

// The library's object for one sensor, on the pin from config.h
static DHT dht(SENSOR_PIN, DHT21);

bool sensorRead(bool coldBoot, float *temperature, float *humidity)
{
    // Sets up the pin. It's safe to call more than once, which happens with DEEP_SLEEP off.
    dht.begin();

    // millis() counts from when the board started, which is also when the sensor got power
    if (coldBoot && millis() < SENSOR_STARTUP_MS) {
        delay(SENSOR_STARTUP_MS - millis());
    }

    // Two tries. A single bad reading now and then is normal for these sensors, from
    // electrical noise or a timing hiccup. Two in a row means something's really wrong.
    for (int attempt = 1; attempt <= 2; attempt++) {
        if (attempt > 1) {
            delay(SENSOR_RETRY_MS);
        }

        // true forces a new reading. Humidity then comes from that same reading, because
        // the library hands back its last result to any request within 2 seconds.
        //
        // The false asks for Celsius. true would mean Fahrenheit.
        float t = dht.readTemperature(false, true);
        float h = dht.readHumidity();

        // A failed read comes back as NaN (not a number), and isnan is how you spot it.
        // NaN doesn't equal anything, not even itself, so == wouldn't work.
        if (!isnan(t) && !isnan(h)) {
            *temperature = t;
            *humidity = h;
            return true;
        }

        LOG("SENSOR", "Reading failed (attempt %d)", attempt);
    }

    return false;
}
