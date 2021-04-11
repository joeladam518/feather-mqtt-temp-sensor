#ifndef __MQTT_TEMP_STATE_CLASS_H__
#define __MQTT_TEMP_STATE_CLASS_H__
#include "config.h"
#include <DHT.h>

class Sensor
{
public:
    Sensor(int pin);

    float getHumidity();
    float getTemperature();
    const char* getTempUnit();
    void read();

protected:
    DHT dht;
    float humidity;
    bool isReading;
    float temperature;
    char* tempUnit;
};

#endif
