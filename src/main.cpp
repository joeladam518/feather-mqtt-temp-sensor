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
    read(&sensorValues);
    publish(PUB_STATE, &sensorValues);

    delay(20000);
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
    float humidity = dht.readHumidity(false);

    if (!isnan(humidity)) {
        values->humidity = humidity;
    }

    delay(250);

    float temperature = dht.readTemperature(true, false);

    if (!isnan(temperature)) {
        values->temperature = temperature;
    }

    delay(250);

#ifdef DEBUG
    Serial.println(F("read()"));
    Serial.print(F("temperature: "));
    Serial.println(values->temperature);
    Serial.print(F("humidity: "));
    Serial.println(values->humidity);
#endif
}

void publish(const char* topic, DhtValues_t* values)
{
#ifdef DEBUG
    Serial.print(F("publishTemp() "));
    Serial.print(F("topic: "));
    Serial.println(topic);
#endif

    char output[60];
    memset(output, '\0', sizeof(output));

    const int capacity = JSON_OBJECT_SIZE(3);
    StaticJsonDocument<capacity> doc;

    doc["Humidity"].set(values->humidity);
    doc["Temperature"].set(values->temperature);
    doc["TempUnit"].set((const char*)values->tempUnit);

    serializeJson(doc, output, sizeof(output));
    mqtt.publish(topic, output);
}
