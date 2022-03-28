#ifndef __MQTT_TEMP_MAIN_H__
#define __MQTT_TEMP_MAIN_H__

struct DhtValues {
    float humidity;
    float temperature;
    char tempUnit[2];
};

void mqttConnect(void);
void read(struct DhtValues* values);
void publish(const char* topic, struct DhtValues* values);

#endif
