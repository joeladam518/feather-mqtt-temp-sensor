// sensor.h: reads the AM2301 temperature and humidity sensor.
//
// One function, so the rest of the code doesn't need to know anything about the sensor
// library. Swapping in a different sensor would only mean changing sensor.cpp.

#pragma once

// Reads both values at once, as they come off the sensor, before any calibration.
// Temperature is in °C and humidity in %. Returns false if the sensor didn't answer, twice.
//
// coldBoot says the board (and so the sensor) just got power, which means waiting for the
// sensor to start up. After a deep sleep the sensor has been powered the whole time.
//
// The readings are written to *temperature and *humidity, and only when it returns true.
bool sensorRead(bool coldBoot, float *temperature, float *humidity);
