// config_example.h: a template for your settings.
//
// Copy this to main/config.h and fill it in. config.h is gitignored, so your passwords never
// get committed, and this file stays safe to share. These are #defines because they're
// fixed when the firmware is built. Changing one means building and flashing again.
//
// The calibration offsets aren't here, because they're set from Home Assistant without
// reflashing. The reporting interval can be changed there too, and this is only its default.
//
// A #define is a find and replace the compiler does before compiling: everywhere the code
// says WLAN_SSID, it gets whatever is written here instead.

#pragma once

// Wifi. The ESP8266 only does 2.4 GHz networks, not 5 GHz.
#define WLAN_SSID ""
#define WLAN_PASS ""

// MQTT. Usually the address of the machine running Home Assistant's Mosquitto add-on.
#define MQTT_BROKER "0.0.0.0"
#define MQTT_PORT   1883
#define MQTT_USER   "" // Leave empty if your broker doesn't need a login
#define MQTT_PASS   ""

// Device
#define DEVICE_ID   "temp_sensor"        // Letters, numbers, and underscores. Must be unique on your broker.
#define DEVICE_NAME "Temperature Sensor" // What Home Assistant calls it
#define TOPIC_BASE  "temp_sensor"        // Every topic starts with this

// The GPIO the AM2301's data wire is connected to
#define SENSOR_PIN 14

// How often to report, in milliseconds, until it's changed from Home Assistant. It has to be
// a whole number of minutes, from 1 to 60. 60000 (1 minute) is handy while testing, and
// 300000 is 5 minutes. Flashing with a different value here resets the interval to it.
#define REPORT_INTERVAL_DEFAULT_MS 300000

// 1 to deep sleep between readings, which is what makes a battery last. It needs a wire from
// GPIO16 to RST, or the board never wakes up. 0 keeps the board awake between readings,
// which is fine on USB power and needs no extra wire.
#define DEEP_SLEEP 1
