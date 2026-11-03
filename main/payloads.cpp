// payloads.cpp: builds everything the device sends to MQTT.
//
// The discovery message is how the device sets itself up in Home Assistant. Instead of you
// writing YAML by hand, the device describes itself: "I'm a device with a temperature
// sensor, a humidity sensor, and three settings, and here are their topics". Home Assistant
// reads that and creates the entities. Shortened, it looks like this:
//
//   {
//     "dev":  {"ids":"temp_sensor", "name":"Temperature Sensor", ...},  the device itself
//     "o":    {"name":"feather-mqtt-temp-sensor", ...},                 the software that sent it
//     "cmps": {                                                         its entities
//       "temperature": {"p":"sensor", "device_class":"temperature", ...},
//       "interval":    {"p":"number", "command_topic":"temp_sensor/interval/set", ...},
//       ...
//     }
//   }
//
// The JSON is built with ArduinoJson, which despite the name works anywhere, including in
// the tests on your computer. You fill in a JsonDocument like a dictionary, doc["key"] =
// value, and it writes out the text.
//
// Like settings.cpp, there's no Arduino code in here, so the tests can compile it.

#include "config.h"
#include "payloads.h"

#include <math.h>

#include <ArduinoJson.h>

#include "topics.h"

// Rounds to one decimal place. The sensor's resolution is 0.1, so more digits would
// only be noise. It returns a double because that's what ArduinoJson prints most neatly:
// 21.4 comes out as 21.4 and not 21.39999962.
static double roundToTenth(float value)
{
    return round((double)value * 10.0) / 10.0;
}

// Better to send nothing than JSON that got cut off halfway.
//
// measureJson works out how long the text will be without writing it. It has to be
// shorter than size, not equal, because serializeJson also writes a NUL at the end.
//
// The & in "JsonDocument &doc" passes the document by reference, which means this
// function works on the caller's document directly instead of on a copy of it.
static size_t serializeChecked(JsonDocument &doc, char *output, size_t size)
{
    if (measureJson(doc) >= size) {
        return 0;
    }

    return serializeJson(doc, output, size);
}

size_t serializeState(float temperature, float humidity, int32_t rssi, char *output, size_t size)
{
    // A failed read comes back as NaN, and NaN isn't valid JSON
    if (!isfinite(temperature) || !isfinite(humidity)) {
        return 0;
    }

    JsonDocument doc;
    doc["temperature"] = roundToTenth(temperature);
    doc["humidity"] = roundToTenth(humidity);
    doc["rssi"] = rssi;

    return serializeChecked(doc, output, size);
}

size_t serializeSettings(const Settings_t *settings, char *output, size_t size)
{
    // These key names have to match the value_templates in the discovery message below,
    // which is how each of Home Assistant's number boxes finds its value. A test checks it.
    JsonDocument doc;
    doc["interval"] = settings->intervalMinutes;
    doc["temperature_offset"] = roundToTenth(settings->temperatureOffset);
    doc["humidity_offset"] = roundToTenth(settings->humidityOffset);

    return serializeChecked(doc, output, size);
}

// The settings are numbers you type in Home Assistant. They all share these options.
//
// "retain" is the important one. Normally a command is sent once, and a device that's
// asleep misses it. With retain, the broker holds on to the command until the device
// wakes up and collects it. The device then clears it, see handleMessage in app.cpp.
//
// It returns the new entity, so the caller can add the options that differ, like the
// limits and the unit.
static JsonObject addSettingNumber(JsonObject components, const char *key, const char *name,
                                   const char *commandTopic, const char *valueTemplate)
{
    // .to<JsonObject>() makes components[key] a new, empty object, and hands it back
    JsonObject number = components[key].to<JsonObject>();
    number["p"] = "number";                  // The platform, meaning what kind of entity it is
    number["name"] = name;                   // What Home Assistant shows next to it
    number["command_topic"] = commandTopic;  // Where Home Assistant sends a new value
    number["state_topic"] = TOPIC_SETTINGS;  // Where it reads the current value from
    number["value_template"] = valueTemplate; // How it picks this value out of the JSON
    number["mode"] = "box";                  // A box you type into, instead of a slider
    number["entity_category"] = "config";    // Listed under Configuration on the device page
    number["retain"] = true;
    return number;
}

// The readings all come from the one state message. Each picks out its own value.
//
// There's no availability topic. A device that sleeps would show as unavailable almost
// all the time. expire_after does the job instead: a reading that's not followed by
// another one in time gets marked unavailable.
static JsonObject addReadingSensor(JsonObject components, const char *key, const char *name,
                                   const char *valueTemplate, uint32_t expireAfter)
{
    JsonObject sensor = components[key].to<JsonObject>();
    sensor["p"] = "sensor";
    sensor["name"] = name;
    sensor["state_topic"] = TOPIC_STATE;
    sensor["value_template"] = valueTemplate; // Jinja, Home Assistant's template language
    sensor["expire_after"] = expireAfter;     // In seconds
    return sensor;
}

// See https://www.home-assistant.io/integrations/mqtt/ (device discovery)
size_t serializeDiscovery(uint16_t intervalMinutes, char *output, size_t size)
{
    JsonDocument doc;
    uint32_t expireAfter = expireAfterSeconds(intervalMinutes);

    // The physical device. Every entity below gets grouped under it.
    // The short names are Home Assistant's abbreviations: ids is identifiers, mf is
    // manufacturer, and mdl is model.
    JsonObject device = doc["dev"].to<JsonObject>();
    device["ids"] = DEVICE_ID;
    device["name"] = DEVICE_NAME;
    device["mf"] = "Adafruit";
    device["mdl"] = "Feather HUZZAH ESP8266 + AM2301";

    // The software that's sending this, which the docs require for device discovery
    JsonObject origin = doc["o"].to<JsonObject>();
    origin["name"] = "feather-mqtt-temp-sensor";
    origin["url"] = "https://github.com/joeladam518/feather-mqtt-temp-sensor";

    // cmps is short for components, Home Assistant's name for the entities
    JsonObject components = doc["cmps"].to<JsonObject>();

    // Readings. Temperature is sent in °C, and the device_class lets Home Assistant
    // convert it to °F if that's what you've set it to show.
    //
    // unique_id is what lets you rename an entity or change its icon in Home Assistant,
    // and have that stick. It has to be different for every entity. DEVICE_ID "_temperature"
    // is two strings side by side, which the compiler joins into one.
    JsonObject temperature = addReadingSensor(components, "temperature", "Temperature",
                                              "{{ value_json.temperature }}", expireAfter);
    temperature["unique_id"] = DEVICE_ID "_temperature";
    temperature["device_class"] = "temperature";
    temperature["unit_of_measurement"] = "°C";
    temperature["state_class"] = "measurement"; // Makes Home Assistant keep long term statistics
    temperature["suggested_display_precision"] = 1;

    JsonObject humidity = addReadingSensor(components, "humidity", "Humidity",
                                           "{{ value_json.humidity }}", expireAfter);
    humidity["unique_id"] = DEVICE_ID "_humidity";
    humidity["device_class"] = "humidity";
    humidity["unit_of_measurement"] = "%";
    humidity["state_class"] = "measurement";
    humidity["suggested_display_precision"] = 1;

    // Wifi signal strength, which helps when a sensor keeps dropping out. Hidden under
    // Diagnostic, and off until you turn it on.
    JsonObject rssi = addReadingSensor(components, "rssi", "Signal strength",
                                       "{{ value_json.rssi }}", expireAfter);
    rssi["unique_id"] = DEVICE_ID "_rssi";
    rssi["device_class"] = "signal_strength";
    rssi["unit_of_measurement"] = "dBm";
    rssi["state_class"] = "measurement";
    rssi["entity_category"] = "diagnostic";
    rssi["enabled_by_default"] = false;

    // Settings. The limits here are the same ones settings.cpp enforces.
    JsonObject interval = addSettingNumber(components, "interval", "Report interval",
                                           TOPIC_INTERVAL_SET, "{{ value_json.interval }}");
    interval["unique_id"] = DEVICE_ID "_interval";
    interval["unit_of_measurement"] = "min";
    interval["min"] = INTERVAL_MIN_MINUTES;
    interval["max"] = INTERVAL_MAX_MINUTES;
    interval["step"] = 1;
    interval["icon"] = "mdi:timer-outline"; // An icon from materialdesignicons.com

    // There's deliberately no device_class here. With device_class temperature, Home
    // Assistant would convert this like a temperature, and an offset of 0 °C would show
    // as 32 °F. An offset is a difference, so it's left in °C.
    JsonObject temperatureOffset = addSettingNumber(components, "temperature_offset", "Temperature offset",
                                                    TOPIC_TEMPERATURE_OFFSET_SET,
                                                    "{{ value_json.temperature_offset }}");
    temperatureOffset["unique_id"] = DEVICE_ID "_temperature_offset";
    temperatureOffset["unit_of_measurement"] = "°C";
    temperatureOffset["min"] = TEMPERATURE_OFFSET_MIN;
    temperatureOffset["max"] = TEMPERATURE_OFFSET_MAX;
    temperatureOffset["step"] = 0.1;
    temperatureOffset["icon"] = "mdi:thermometer-lines";

    JsonObject humidityOffset = addSettingNumber(components, "humidity_offset", "Humidity offset",
                                                 TOPIC_HUMIDITY_OFFSET_SET,
                                                 "{{ value_json.humidity_offset }}");
    humidityOffset["unique_id"] = DEVICE_ID "_humidity_offset";
    humidityOffset["unit_of_measurement"] = "%";
    humidityOffset["min"] = HUMIDITY_OFFSET_MIN;
    humidityOffset["max"] = HUMIDITY_OFFSET_MAX;
    humidityOffset["step"] = 0.1;
    humidityOffset["icon"] = "mdi:water-percent";

    return serializeChecked(doc, output, size);
}
