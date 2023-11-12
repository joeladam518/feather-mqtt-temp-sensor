#ifndef __FEATHER_MQTT_LED_CONFIG_H__
#define __FEATHER_MQTT_LED_CONFIG_H__

// Debug
#define DEBUG false

// Wifi
#define WLAN_SSID ""
#define WLAN_PASS ""

// MQTT
#define MQTT_BROKER ""
#define MQTT_PORT   1883 // Use 8883 for SSL
#define MQTT_USER   ""
#define MQTT_PASS   ""

// Sensor
#define TEMP_SENSOR_PIN 0

// Subscription Topics
#define SUB_GET_TEMP ""

// Publish Topics
#define PUB_STATE ""
#define PUB_STATUS ""

#endif
