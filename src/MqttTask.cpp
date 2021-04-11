#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_MQTT_Client.h>
#include "MqttTask.h"
#include "PublishTemp.hpp"
#include "Sensor.h"

//-------------------------------
// Constructor
//-------------------------------
MqttTask::MqttTask(Adafruit_MQTT_Client* m, Sensor* s):
    mqtt(m),
    sensor(s),
    getTempSubscription(m, SUB_GET_TEMP)
{
    //
}

//-------------------------------
// Destructor
//-------------------------------
// MqttTask::~MqttTask() {}

//-------------------------------
// Methods
//-------------------------------
void MqttTask::setup()
{
    Serial.println(F("Setup MqttTask"));

    // unsubscribe
    mqtt->unsubscribe(&getTempSubscription);
    // then re-subscribe
    mqtt->subscribe(&getTempSubscription);
}

void MqttTask::loop()
{
    Serial.println(F("Run MqttTask"));

    connect();

    processPackets(5000);

    if(!mqtt->ping()) {
        mqtt->disconnect();
    }

    delay(150);
}

void MqttTask::connect()
{
    if (mqtt->connected()) {
        return;
    }

    Serial.print("Connecting to MQTT... ");

    int8_t ret;
    uint8_t retries = 3;
    while ((ret = mqtt->connect()) != 0) {
        Serial.println(mqtt->connectErrorString(ret));
        Serial.println("Retrying MQTT connection in 5 seconds...");

        mqtt->disconnect();
        delay(5000);

        retries--;
        if (retries == 0) {
            while (1); // die and wait  to be reset
        }
    }

    Serial.println("MQTT Connected!");
}

bool MqttTask::shouldRun()
{
    return Task::shouldRun();
}

void MqttTask::processPackets(int16_t readTime)
{
    if (!mqtt->connected()) {
        return;
    }

    Adafruit_MQTT_Subscribe *subscription = mqtt->readSubscription(readTime);

    if (subscription == NULL) {
        return;
    }

    if (subscription == &getTempSubscription) {
        publishTemp(PUB_STATUS, mqtt, sensor);
    }
}
