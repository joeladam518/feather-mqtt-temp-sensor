#include "config.h"
#include <Arduino.h>
#include <DHT.h>
#include <HardwareSerial.h>
#include "Sensor.h"

Sensor::Sensor(int pin): dht(pin, DHT21)
{
    isReading = false;
    humidity = 0.00;
    temperature = 0.00;
    tempUnit = (char*)"F";

    dht.begin();
}

float Sensor::getHumidity()
{
    return humidity;
}

float Sensor::getTemperature()
{
    return temperature;
}

const char* Sensor::getTempUnit()
{
    return (const char*)tempUnit;
}

void Sensor::read()
{
    if (isReading) {
        return;
    }

    isReading = true;

    humidity = dht.readHumidity(false);
    temperature = dht.readTemperature(true, false);

    isReading = false;
}
