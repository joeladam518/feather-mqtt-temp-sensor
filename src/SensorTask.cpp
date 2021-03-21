#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ArduinoJson.h>
#include "SensorTask.h"


//-------------------------------
// Constructor
//-------------------------------
SensorTask::SensorTask(Adafruit_MQTT_Client* m, int pin): mqtt(m), dht(pin, DHT21)
{
    memset(output, '\0', sizeof(output));
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
    //Serial.println("Setup Sensor Task");
    dht.begin();
}

void SensorTask::loop()
{
    //Serial.println("Run SensorTask");
    memset(output, '\0', sizeof(output));

    const int capacity = JSON_OBJECT_SIZE(3);
    StaticJsonDocument<capacity> doc;

    // note: readTemperature(true, false) === read temp in fahrenheit
    float temperature = dht.readTemperature(true, false);
    float humidity = dht.readHumidity(false);

    doc["Temperature"].set(temperature);
    doc["TempUnit"].set("F");
    doc["Humidity"].set(humidity);

    serializeJson(doc, output, sizeof(output));
    mqtt->publish(PUB_STATE, output);

    delay(60300);
}

bool SensorTask::shouldRun()
{
    return (mqtt->connected() && Task::shouldRun());
}
