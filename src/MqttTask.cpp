#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include "MqttTask.h"

//-------------------------------
// Constructor
//-------------------------------
MqttTask::MqttTask(Adafruit_MQTT_Client* m): mqtt(m)
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
    //Serial.println("Setup MqttTask");
}

void MqttTask::loop()
{
    //Serial.println("Run MqttTask");

    connect();

    mqtt->processPackets(10000);

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
