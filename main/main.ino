// main.ino: where the firmware starts.
//
// arduino-cli insists that a sketch's main file is named after its folder, which is why
// this folder is main/ and this file is main.ino. It's deliberately almost empty. The real
// work is in app.cpp.
//
// An .ino file is C++ with a little Arduino magic added: arduino-cli adds
// #include <Arduino.h> to the top before compiling it. Every other file in main/ is plain
// C++, and gets compiled and linked in alongside this one.
//
// Reading order if you're new to this code:
//   1. app.cpp       Everything the device does for each reading, in order
//   2. settings.cpp  The settings you can change from Home Assistant, and their rules
//   3. payloads.cpp  What gets sent to MQTT, including the Home Assistant discovery message
//   4. network.cpp   Wifi and the MQTT connection
//   5. sensor.cpp    Reading the AM2301
//   6. storage.cpp   What the device remembers between readings
//
// The headers (.h) next to each .cpp list what it offers, with a comment on each function.

#include "app.h"

// Arduino calls setup() once at startup. With DEEP_SLEEP on, waking from deep sleep is a
// full restart, so setup() runs once per reading, and it never returns because it ends by
// putting the board to sleep.
void setup()
{
    appSetup();
}

// Arduino calls loop() over and over after setup(). It's only reached with DEEP_SLEEP off,
// and that's where the device waits for the next reading.
void loop()
{
    appLoop();
}
