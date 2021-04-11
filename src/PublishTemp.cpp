#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <Adafruit_MQTT_Client.h>
#include "Sensor.h"
#include "PublishTemp.hpp"

void publishTemp(const char* topic, Adafruit_MQTT_Client* mqtt, Sensor* sensor)
{
    Serial.print(F("publishTemp() "));
    Serial.print(F("Topic: "));
    Serial.println(topic);

    sensor->read();

    char output[60];
    memset(output, '\0', sizeof(output));

    const int capacity = JSON_OBJECT_SIZE(3);
    StaticJsonDocument<capacity> doc;

    doc["Humidity"].set(sensor->getHumidity());
    doc["Temperature"].set(sensor->getTemperature());
    doc["TempUnit"].set(sensor->getTempUnit());

    serializeJson(doc, output, sizeof(output));
    mqtt->publish(topic, output);
}
