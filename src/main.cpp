#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Scheduler.h>
#include "MqttTask.h"
#include "Sensor.h"
#include "SensorTask.h"

#define TEMP_SENSOR_PIN 14

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_BROKER, MQTT_PORT);

Sensor sensor(TEMP_SENSOR_PIN);

MqttTask mqttTask(&mqtt, &sensor);
SensorTask sensorTask(&mqtt, &sensor);

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println("");

    pinMode(TEMP_SENSOR_PIN, OUTPUT);
    digitalWrite(TEMP_SENSOR_PIN, LOW);

    WiFi.begin(WLAN_SSID, WLAN_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    Scheduler.start(&sensorTask);
    Scheduler.start(&mqttTask);

    Scheduler.begin();
}

void loop() {}
