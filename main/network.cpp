// network.cpp: wifi, and the connection to the MQTT broker.
//
// A quick MQTT refresher: devices don't talk to each other directly. They all connect to a
// broker, publish messages to named topics, and subscribe to the topics they care about.
//
//   QoS        Quality of service. 0 means a message is sent once, with no check that it
//              arrived. 1 means it's resent until it's acknowledged.
//   Retained   The broker keeps the last message on a topic, and gives it to anyone who
//              subscribes later. It's what makes a sleeping device workable: Home Assistant
//              can send a setting while the device is asleep, and the broker hands it over
//              when the device wakes up and subscribes.
//
// The wifi side comes from the ESP8266 Arduino core (ESP8266WiFi.h), and the MQTT side from
// the PubSubClient library.

#include "config.h"
#include "network.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <string.h>

#include "logging.h"

// For DISCOVERY_PAYLOAD_MAX, which decides the MQTT buffer size
#include "payloads.h"

// Connecting straight to a saved access point takes well under a second when it works.
// If it hasn't worked by now, the access point probably moved channel, so scan instead.
#define FAST_CONNECT_TIMEOUT_MS 5000

// How long to wait for the broker to answer
#define MQTT_TIMEOUT_MS 5000

// PubSubClient builds each outgoing message in one buffer, so it has to fit the biggest
// one, which is discovery. The extra is room for the topic and the message header.
#define MQTT_BUFFER_SIZE (DISCOVERY_PAYLOAD_MAX + 128)

// WiFiClient is the TCP connection, and PubSubClient speaks MQTT over it.
//
// These are objects from C++ libraries. mqtt is built around wifiClient, which is why
// wifiClient is passed in when it's created. Being outside any function, both are created
// once when the board starts, and live for as long as it's running.
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

//==============================================================================
// Wifi

bool wifiConnect(RtcState_t *rtc, uint32_t timeoutMs)
{
    // Remember when we started, to time out and to log how long it took
    uint32_t start = millis();

    // By default the ESP8266 saves the wifi settings to flash on every connect. They're
    // compiled in anyway, and writing flash every few minutes would wear it out.
    WiFi.persistent(false);

    // STA (station) mode means we join a network, instead of being an access point
    WiFi.mode(WIFI_STA);

    // Telling it the channel and the access point's address lets it skip the scan.
    // A channel of 0 means we don't have them, either because it's the first time or
    // because the last attempt failed.
    bool fast = rtc->wifiChannel != 0;
    if (fast) {
        WiFi.begin(WLAN_SSID, WLAN_PASS, rtc->wifiChannel, rtc->wifiBssid, true);
    } else {
        WiFi.begin(WLAN_SSID, WLAN_PASS);
    }

    // WiFi.begin only starts connecting. The connecting happens in the background, so
    // check on it every 20 ms until it's done or we run out of time.
    while (WiFi.status() != WL_CONNECTED) {
        // Subtracting the start time like this still works when millis() wraps around
        uint32_t elapsed = millis() - start;

        if (elapsed >= timeoutMs) {
            LOG("WIFI", "Couldn't connect to %s", WLAN_SSID);

            // Forget the saved access point, so the next attempt does a full scan
            rtc->wifiChannel = 0;
            return false;
        }

        // Only switches over once, because fast is set to false here
        if (fast && elapsed >= FAST_CONNECT_TIMEOUT_MS) {
            LOG("WIFI", "The saved access point didn't answer, scanning instead");
            fast = false;
            WiFi.disconnect();
            WiFi.begin(WLAN_SSID, WLAN_PASS);
        }

        // delay() also lets the wifi stack run. Spinning in a tight loop without it
        // would stop the connection from ever happening.
        delay(20);
    }

    // Save this access point's details, for a fast connect next time.
    // BSSID is the access point's MAC address, which tells two access points with the
    // same network name apart.
    rtc->wifiChannel = (uint8_t)WiFi.channel();
    memcpy(rtc->wifiBssid, WiFi.BSSID(), sizeof(rtc->wifiBssid));

    LOG("WIFI", "Connected in %lu ms, IP address %s", (unsigned long)(millis() - start),
        WiFi.localIP().toString().c_str());
    return true;
}

// Signal strength, in dBm. Closer to 0 is better: -50 is great, -80 is barely hanging on.
int32_t wifiRssi(void)
{
    return WiFi.RSSI();
}

void wifiStop(void)
{
    // true also turns the radio off
    WiFi.disconnect(true);
}

//==============================================================================
// MQTT

bool mqttConnect(MessageHandler_t handler)
{
    uint32_t start = millis();

    // Where the broker is, and who to call when a message arrives
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(handler);

    // Don't wait forever for a broker that isn't answering. PubSubClient counts in
    // seconds, and WiFiClient in milliseconds.
    mqtt.setSocketTimeout(MQTT_TIMEOUT_MS / 1000);
    wifiClient.setTimeout(MQTT_TIMEOUT_MS);

    // The default buffer is 256 bytes, far too small for discovery. This sets aside memory
    // for a bigger one, and fails if there isn't enough free.
    if (!mqtt.setBufferSize(MQTT_BUFFER_SIZE)) {
        LOG("MQTT", "Not enough memory for a %d byte buffer", MQTT_BUFFER_SIZE);
        return false;
    }

    // Leave MQTT_USER empty in config.h for a broker without a login. NULL tells
    // PubSubClient not to send one.
    //
    // condition ? a : b picks a when the condition is true, and b when it's false
    const char *user = strlen(MQTT_USER) > 0 ? MQTT_USER : NULL;
    const char *pass = strlen(MQTT_USER) > 0 ? MQTT_PASS : NULL;

    // DEVICE_ID doubles as the client ID, which the broker uses to tell clients apart.
    // Two devices with the same ID would keep kicking each other off.
    if (!mqtt.connect(DEVICE_ID, user, pass)) {
        // state() is a number. PubSubClient.h lists them: -2 means the broker couldn't be
        // reached at all, 4 and 5 mean the login was refused.
        LOG("MQTT", "Couldn't connect to %s:%d (state %d)", MQTT_BROKER, MQTT_PORT, mqtt.state());
        return false;
    }

    LOG("MQTT", "Connected in %lu ms", (unsigned long)(millis() - start));
    return true;
}

bool mqttSubscribe(const char *topic)
{
    // QoS 1, so the broker resends anything we don't acknowledge
    return mqtt.subscribe(topic, 1);
}

void mqttListen(uint32_t durationMs)
{
    uint32_t start = millis();

    // loop() reads whatever has arrived and passes each message to the handler. It returns
    // false once the connection has dropped, and then there's nothing left to wait for.
    while (millis() - start < durationMs) {
        if (!mqtt.loop()) {
            return;
        }
        delay(10);
    }
}

bool mqttPublish(const char *topic, const char *payload, bool retain)
{
    // PubSubClient always publishes at QoS 0, so this only tells us the message was handed
    // to the network, not that the broker got it. mqttDisconnect is what waits for that.
    if (!mqtt.publish(topic, payload, retain)) {
        LOG("MQTT", "Publishing to %s failed", topic);
        return false;
    }

    return true;
}

bool mqttClearRetained(const char *topic)
{
    // An empty payload: zero bytes long, retained. The cast is because this version of
    // publish() takes the payload as raw bytes.
    return mqtt.publish(topic, (const uint8_t *)"", 0, true);
}

void mqttDisconnect(void)
{
    // Says goodbye to the broker, then closes the connection. Closing it waits until the
    // broker has acknowledged everything we sent. Going to sleep with messages still in
    // flight would lose them.
    mqtt.disconnect();
}
