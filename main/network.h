// network.h: wifi, and the connection to the MQTT broker.
//
// Everything here gives up after a time limit instead of retrying forever. On battery,
// a device stuck trying to connect would drain it in hours. It's better to sleep, and
// try again at the next reading.
//
// The rest of the code only talks to the network through these functions. That keeps the
// wifi and MQTT library details in network.cpp, out of everyone else's way.

#pragma once

#include <stdint.h>

// For RtcState_t, where the wifi details are remembered
#include "storage.h"

// Called for each incoming message. The payload is not NUL terminated, so use the length.
//
// This is a function pointer type: a variable of type MessageHandler_t holds the address
// of a function with exactly this shape. It's how app.cpp tells network.cpp which of its
// functions to call when a message arrives.
typedef void (*MessageHandler_t)(char *topic, uint8_t *payload, unsigned int length);

// Connects to wifi. Uses the access point saved in rtc to skip the scan when it can, and
// saves the one it connected to for next time. Returns false if it gave up.
bool wifiConnect(RtcState_t *rtc, uint32_t timeoutMs);

// The current wifi signal strength in dBm, for the state message
int32_t wifiRssi(void);

// Disconnects, and turns the radio off to save power
void wifiStop(void);

// Connects to the broker set in config.h. handler gets called for every message that
// arrives from then on. Returns false if the broker couldn't be reached or refused us.
bool mqttConnect(MessageHandler_t handler);

// Asks the broker to send us messages on this topic, including any retained one
bool mqttSubscribe(const char *topic);

// Keeps the connection running for this long, which is when incoming messages get delivered
// to the handler. Stops early if the connection drops.
void mqttListen(uint32_t durationMs);

// Sends a message. With retain, the broker keeps it and hands it to anyone who subscribes
// later. Returns false if it couldn't be sent.
bool mqttPublish(const char *topic, const char *payload, bool retain);

// Deletes the retained message on a topic. An empty retained message is how MQTT says
// "forget what you were holding".
bool mqttClearRetained(const char *topic);

// Says goodbye to the broker and closes the connection, once everything sent has arrived
void mqttDisconnect(void);
