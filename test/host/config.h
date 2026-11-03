// config.h for the tests.
//
// The code being tested includes "config.h", like the firmware does. scripts/test-host.sh
// makes sure it finds this one instead of your real main/config.h, so the tests always run
// with the same names and never see your passwords.
//
// Only the settings that settings.cpp and payloads.cpp use are needed here. The wifi and
// MQTT ones aren't, because nothing being tested connects to anything.

#pragma once

// Fixed settings for the tests, scripts/test-host.sh keeps your real config.h out
#define DEVICE_ID   "test_sensor"
#define DEVICE_NAME "Test Sensor"
#define TOPIC_BASE  "test/sensor"

// 5 minutes, so the tests can check the default is what the README promises
#define REPORT_INTERVAL_DEFAULT_MS 300000
