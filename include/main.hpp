#ifndef __MQTT_TEMP_MAIN_H__
#define __MQTT_TEMP_MAIN_H__

typedef struct DhtValues {
    float humidity;
    float temperature;
    char tempUnit[2];
} DhtValues_t;

void mqttConnect(void);
void read(DhtValues_t* values);
void publish(const char* topic, DhtValues_t* values);

#endif
