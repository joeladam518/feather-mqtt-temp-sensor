#ifndef __MQTT_TEMP_PUBLISH_TEMP_H__
#define __MQTT_TEMP_PUBLISH_TEMP_H__
#include <Adafruit_MQTT_Client.h>
#include "Sensor.h"

void publishTemp(const char* topic, Adafruit_MQTT_Client* mqtt, Sensor* sensor);

#endif
