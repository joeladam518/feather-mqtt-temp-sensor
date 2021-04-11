#ifndef __MQTT_TEMP_SENSOR_TASK_H__
#define __MQTT_TEMP_SENSOR_TASK_H__
#include "config.h"
#include <Arduino.h>
#include <Adafruit_MQTT_Client.h>
#include <DHT.h>
#include <Scheduler.h>
#include "Sensor.h"

class SensorTask : public Task
{
public:
    SensorTask(Adafruit_MQTT_Client* m, Sensor* s);

protected:
    Adafruit_MQTT_Client* mqtt;
    Sensor* sensor;

    void loop();
    void setup();
    bool shouldRun();
};

#endif
