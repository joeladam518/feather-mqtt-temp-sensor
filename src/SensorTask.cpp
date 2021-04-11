#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "PublishTemp.hpp"
#include "Sensor.h"
#include "SensorTask.h"


//-------------------------------
// Constructor
//-------------------------------
SensorTask::SensorTask(Adafruit_MQTT_Client* m, Sensor* s):
    mqtt(m),
    sensor(s)
{
    //
}


//-------------------------------
// SensorTask
//-------------------------------
// SensorTask::~SensorTask() {}


//-------------------------------
// Methods
//-------------------------------
void SensorTask::setup()
{
    Serial.println(F("Setup SensorTask"));
}

void SensorTask::loop()
{
    Serial.println(F("Run SensorTask"));

    publishTemp(PUB_STATE, mqtt, sensor);

    delay(60300);
}

bool SensorTask::shouldRun()
{
    return (mqtt->connected() && Task::shouldRun());
}
