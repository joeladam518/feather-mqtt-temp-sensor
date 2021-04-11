#ifndef __MQTT_TEMP_MQTT_TASK_H__
#define __MQTT_TEMP_MQTT_TASK_H__
#include "config.h"
#include <Adafruit_MQTT_Client.h>
#include <Scheduler.h>
#include "Sensor.h"

class MqttTask : public Task
{
public:
    MqttTask(Adafruit_MQTT_Client* m, Sensor* s);

protected:
    Adafruit_MQTT_Client* mqtt;
    Adafruit_MQTT_Subscribe getTempSubscription;
    Sensor* sensor;

    void connect();
    void loop();
    void processPackets(int16_t timeout);
    void setup();
    bool shouldRun();
};

#endif
