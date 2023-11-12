#include "config.h"
#include "main.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <DHT.h>

DHT dht(TEMP_SENSOR_PIN, DHT21);

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_BROKER, MQTT_PORT);
struct DhtValues sensorValues = {0.00, 0.00, "F"};

// Timing
const int period = 30000;
unsigned long now = 0;

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println("");

    WiFi.begin(WLAN_SSID, WLAN_PASS);
    delay(1000);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("Wifi Connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    dht.begin();
}

void loop()
{
    mqttConnect();

    if (millis() >= now + period) {
        now = now + period;
        read(&sensorValues);
        publish(PUB_STATE, &sensorValues);
    }
}

void mqttConnect(void)
{
    if (mqtt.connected()) {
        return;
    }

    Serial.print("Connecting to MQTT... ");

    int8_t ret;
    uint8_t retries = 3;
    while ((ret = mqtt.connect()) != 0) {
        Serial.println(mqtt.connectErrorString(ret));
        Serial.println("Retrying MQTT connection in 5 seconds...");

        mqtt.disconnect();
        delay(5000);

        retries--;
        if (retries == 0) {
            while (1); // die and wait  to be reset
        }
    }

    Serial.println("MQTT Connected!");
}

void read(DhtValues_t* values)
{
    values->temperature = dht.readTemperature(true);
    values->humidity = dht.readHumidity();

    #ifdef DEBUG
        Serial.println(F("read()"));
        Serial.print(F("temperature: "));
        Serial.println(values->temperature);
        Serial.print(F("humidity: "));
        Serial.println(values->humidity);
    #endif

    delay(200);
}

void publish(const char* topic, DhtValues_t* values)
{
    #ifdef DEBUG
        Serial.println(F("publishTemp()"));
        Serial.print(F("topic: "));
        Serial.println(topic);
    #endif

    char output[60];
    memset(output, '\0', sizeof(output));

    const int capacity = JSON_OBJECT_SIZE(3);
    StaticJsonDocument<capacity> doc;

    doc["Humidity"] = ((int)(values->humidity * 100) / 100.0);
    doc["Temperature"] = ((int)(values->temperature * 100) / 100.0);
    doc["TempUnit"] = (const char *)values->tempUnit;

    serializeJson(doc, output, sizeof(output));
    mqtt.publish(topic, output);
    delay(200);
}
