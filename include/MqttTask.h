#ifndef __MQTT_TEMP_MQTT_TASK_H__
#define __MQTT_TEMP_MQTT_TASK_H__
#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Scheduler.h>

class MqttTask : public Task
{
public:
    MqttTask(Adafruit_MQTT_Client* m);

protected:
    Adafruit_MQTT_Client* mqtt;

    void connect();
    void loop();
    void setup();
    bool shouldRun();
};

#endif
