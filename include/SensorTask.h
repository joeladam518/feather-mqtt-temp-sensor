#ifndef __MQTT_TEMP_SENSOR_TASK_H__
#define __MQTT_TEMP_SENSOR_TASK_H__
#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <DHT.h>
#include <Scheduler.h>

class SensorTask : public Task
{
public:
    SensorTask(Adafruit_MQTT_Client* m, int pin);

protected:
    Adafruit_MQTT_Client* mqtt;
    char output[SUBSCRIPTIONDATALEN];
    DHT dht;

    void loop();
    void setup();
    bool shouldRun();
};

#endif
