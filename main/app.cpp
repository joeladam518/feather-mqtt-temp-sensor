// app.cpp: what the device does for each reading.
//
// This is the file to read first. Everything else is a helper that this file calls.
//
// With DEEP_SLEEP on, the device spends almost all its time in deep sleep, where everything
// is off except a timer. When the timer runs out, it pulls the reset pin (that's what the
// wire from GPIO16 to RST is for), and the board starts up from scratch. So every reading
// is a fresh boot, and this is the whole life of one:
//
//   1. Load the saved settings
//   2. Read the sensor, before wifi is on, because the radio is electrically noisy
//   3. Connect to wifi and MQTT
//   4. Collect any settings Home Assistant sent while we were asleep, and save them
//   5. Tell Home Assistant about the device, if it hasn't been told yet
//   6. Send the reading
//   7. Sleep until the next one
//
// With DEEP_SLEEP off, the board stays awake, and step 7 waits in loop() instead. Steps 2
// to 6 then repeat every interval, without a reboot.
//
// If anything fails, the rest gets skipped and the device sleeps anyway. Trying again at
// the next reading is better than draining the battery trying now.
//
// The numbers 1 to 7 above appear as comments in the code below, so you can match each step
// to the code that does it.

// Our own header first. If app.h forgot to include something it needs, this is where the
// build would complain, instead of the mistake being hidden by the includes after it.
#include "app.h"

// <Arduino.h> gives us Serial, millis(), delay() and ESP (the object for chip-level
// things like deep sleep). Angle brackets mean "look in the system and library folders".
#include <Arduino.h>

// math.h for NAN, the "not a number" value used for a reading we don't have
#include <math.h>

// string.h for strlen() and memset()
#include <string.h>

// The Espressif SDK underneath Arduino, for the reset reason. It's written in C, and
// already wraps itself in extern "C".
#include <user_interface.h>

// Quotes mean "look next to this file first", which is where our own headers live
#include "logging.h"
#include "network.h"
#include "payloads.h"
#include "sensor.h"
#include "settings.h"
#include "storage.h"
#include "topics.h"

// On unless config.h says otherwise, so a config.h from before this setting existed
// keeps working the way it did.
//
// #ifndef means "if this hasn't been defined yet". config.h gets included (through
// topics.h) before this point, so if it defines DEEP_SLEEP, that wins and this is skipped.
#ifndef DEEP_SLEEP
#define DEEP_SLEEP 1
#endif

// How long to keep trying to join wifi, in milliseconds. 15 seconds is generous: it usually
// takes about 1.
#define WIFI_TIMEOUT_MS 15000

// How long to wait for retained settings after subscribing. The broker sends them straight
// away, so this is plenty, but there's no message that says "that's all of them".
#define COMMAND_WAIT_MS 1000

// The topic each setting's commands arrive on, in SettingId_t order. That means
// SET_TOPICS[SETTING_INTERVAL] is the interval's topic, and so on, so a loop over the
// settings can find each one's topic by its number.
//
// "const char *const" reads right to left: a constant pointer to constant characters.
// Neither the list nor the text in it can be changed.
static const char *const SET_TOPICS[SETTING_COUNT] = {
    TOPIC_INTERVAL_SET,
    TOPIC_TEMPERATURE_OFFSET_SET,
    TOPIC_HUMIDITY_OFFSET_SET,
};

// A command that's arrived but hasn't been applied yet
typedef struct PendingCommand {
    bool received; // A command for this setting arrived
    bool valid;    // False if the payload wasn't a number
    float value;   // The number, if it was one
} PendingCommand_t;

// One slot per setting, filled in by handleMessage and used up by applyCommands.
//
// "static" outside a function means "only visible in this file". Nothing else in the
// firmware can reach in and change these, which makes the code easier to reason about.
static PendingCommand_t pending[SETTING_COUNT];

// Too big to want on the stack. A variable outside any function gets its memory set aside
// when the firmware is built.
//
// The stack is the memory a function's local variables live in, and on the ESP8266 it's
// only 4 KB for all of our code put together. A 4 KB buffer there would use all of it.
static char discoveryPayload[DISCOVERY_PAYLOAD_MAX];

//==============================================================================
// Settings from Home Assistant

// Called by the MQTT library for each message that arrives. It only records the commands.
// They get applied after all of them have arrived, in applyCommands.
//
// The library hands us the message as raw bytes (uint8_t is a byte) and a length. The
// topic is a normal NUL-terminated string, but the payload isn't, which is why the length
// matters.
static void handleMessage(char *topic, uint8_t *payload, unsigned int length)
{
    // An empty message is a cleared command, usually our own from applyCommands coming
    // back to us, since we're subscribed to the topic we cleared
    if (length == 0) {
        return;
    }

    size_t topicLength = strlen(topic);

    // Find which setting this topic belongs to
    for (int i = 0; i < SETTING_COUNT; i++) {
        if (topicEquals(SET_TOPICS[i], topic, topicLength)) {
            pending[i].received = true;

            // The cast tells the compiler to treat the bytes as characters, which they are
            pending[i].valid = parseNumber((const char *)payload, length, &pending[i].value);

            if (!pending[i].valid) {
                // %.*s prints exactly `length` characters. Plain %s would keep reading
                // until it found a NUL, which the payload doesn't have.
                LOG("APP", "Ignoring %s: %.*s is not a number", topic, (int)length, (const char *)payload);
            }
            return;
        }
    }

    // A message on any other topic is ignored. We only subscribe to the three above, so
    // this shouldn't happen.
}

// Applies the commands that arrived, and returns true if any setting changed.
//
// Each command gets cleared off the broker once it's been handled, even one that was
// invalid, or that didn't change anything. Otherwise the broker would keep handing over
// the same old command every time we wake, and it would undo any newer change.
static bool applyCommands(Settings_t *settings)
{
    bool changed = false;

    for (int i = 0; i < SETTING_COUNT; i++) {
        // Nothing arrived for this setting, so leave it as it is
        if (!pending[i].received) {
            continue;
        }

        // applySetting returns true only if the value is really different. Setting the
        // interval to 5 when it's already 5 isn't a change, and doesn't cause a flash write.
        // (SettingId_t)i turns the loop counter back into the enum type applySetting wants.
        if (pending[i].valid && applySetting(settings, (SettingId_t)i, pending[i].value)) {
            LOG("APP", "Changed by %s", SET_TOPICS[i]);
            changed = true;
        }

        mqttClearRetained(SET_TOPICS[i]);
    }

    return changed;
}

//==============================================================================
// Publishing

// Sends discovery and the current settings. Both are retained, so Home Assistant gets them
// whenever it starts up, even though we're asleep. Returns true if both went out.
static bool announce(const Settings_t *settings)
{
    // Small enough for the stack, unlike discovery
    char settingsPayload[SETTINGS_PAYLOAD_MAX];

    // The serialize functions return 0 if the JSON didn't fit. Sending half a message
    // would be worse than sending nothing.
    if (serializeDiscovery(settings->intervalMinutes, discoveryPayload, sizeof(discoveryPayload)) == 0) {
        LOG("APP", "Discovery didn't fit in its buffer");
        return false;
    }

    if (serializeSettings(settings, settingsPayload, sizeof(settingsPayload)) == 0) {
        LOG("APP", "Settings didn't fit in their buffer");
        return false;
    }

    // The & is deliberate. && would skip the second publish if the first one failed.
    return mqttPublish(HA_DISCOVERY_TOPIC, discoveryPayload, true)
         & mqttPublish(TOPIC_SETTINGS, settingsPayload, true);
}

// Retained, so Home Assistant has a value to show straight after it restarts
static void publishReading(float temperature, float humidity)
{
    char payload[STATE_PAYLOAD_MAX];

    // The wifi signal strength goes in too, which helps when a sensor keeps dropping out
    if (serializeState(temperature, humidity, wifiRssi(), payload, sizeof(payload)) > 0) {
        mqttPublish(TOPIC_STATE, payload, true);
    }
}

//==============================================================================
// One reading

// The settings and the RTC state live here, rather than inside a function, because with
// DEEP_SLEEP off they have to last from one reading to the next
static Settings_t settings;
static RtcState_t rtc;

// When the current reading started, by millis(). Only used with DEEP_SLEEP off.
//
// millis() is the number of milliseconds since the board started. It's a uint32_t, an
// unsigned 32 bit number, so it counts up to about 4.3 billion (49 days) and then starts
// again from 0.
static uint32_t readingStartedMs = 0;

// Steps 2 to 6, plus turning wifi off at the end.
//
// coldBoot is true on the first reading after the board gets power, which is when the
// sensor needs a couple of seconds to start up.
static void takeReading(bool coldBoot)
{
    // 2. The sensor. The offsets are applied here, but the settings might still change in
    // step 4. A changed offset is used from the next reading on.
    //
    // Everything starts as NAN, so if the reading fails nothing looks like a real value
    float rawTemperature = NAN;
    float rawHumidity = NAN;
    float temperature = NAN;
    float humidity = NAN;

    // The & in &rawTemperature passes the variable's address, so sensorRead can write the
    // reading into it. That's how a C function hands back more than one value.
    bool haveReading = sensorRead(coldBoot, &rawTemperature, &rawHumidity);

    if (haveReading) {
        applyCalibration(&settings, rawTemperature, rawHumidity, &temperature, &humidity);

        // %.1f prints a float with one decimal place. %% prints a single % sign.
        LOG("APP", "Temperature %.1f °C (raw %.1f), humidity %.1f %% (raw %.1f)",
            temperature, rawTemperature, humidity, rawHumidity);
    }

    // 3. The network. && stops at the first false, so if wifi fails, MQTT isn't even tried,
    // and everything inside the braces is skipped.
    if (wifiConnect(&rtc, WIFI_TIMEOUT_MS) && mqttConnect(handleMessage)) {
        // 4. Subscribing is what makes the broker hand over the retained commands.
        // Forget the last reading's commands first, they've already been applied.
        //
        // memset fills memory with a value, here 0, which sets every received and valid
        // flag to false.
        memset(pending, 0, sizeof(pending));
        for (int i = 0; i < SETTING_COUNT; i++) {
            mqttSubscribe(SET_TOPICS[i]);
        }

        // The commands arrive during this wait, through handleMessage
        mqttListen(COMMAND_WAIT_MS);

        bool changed = applyCommands(&settings);
        if (changed) {
            // Only written when something changed, because flash wears out
            saveSettings(&settings);
            LOG("APP", "Saved: interval %u min, temperature offset %.1f °C, humidity offset %.1f %%",
                settings.intervalMinutes, settings.temperatureOffset, settings.humidityOffset);
        }

        // 5. Discovery includes the interval, so it gets sent again when that changes.
        // If announcing fails, rtc.announced stays false, and it's tried again next time.
        if (changed || !rtc.announced) {
            rtc.announced = announce(&settings);
        }

        // 6. Send the reading, if there is one. A failed reading sends nothing, and Home
        // Assistant shows the last good one until it expires.
        if (haveReading) {
            publishReading(temperature, humidity);
        }

        mqttDisconnect();
    }

    // Turning the radio off saves power, whether we're about to sleep or just wait
    wifiStop();

    // Remembers the access point (for a faster connect next time) and whether we've
    // announced, across the deep sleep
    saveRtcState(&rtc);
}

//==============================================================================
// Sleep

// #if leaves this out of the build entirely when DEEP_SLEEP is 0. Unlike a normal if, it's
// decided when compiling, not while running.
#if DEEP_SLEEP
static void sleepUntilNextReading(uint16_t intervalMinutes)
{
    // millis() here is how long we've been awake, because the board started from scratch
    // at the beginning of this reading
    uint64_t sleepUs = sleepMicroseconds(intervalMinutes, millis());

    // The ESP8266's sleep timer can only count so far. It's a few hours, well past the
    // longest interval, but the timer would wrap around and wake early if it were asked
    // for more.
    uint64_t maxUs = ESP.deepSleepMax();
    if (sleepUs > maxUs) {
        sleepUs = maxUs;
    }

    // printf's %lu is for unsigned long, so the values are cast to match. Passing a
    // different type than the format expects prints garbage.
    LOG("APP", "Awake for %lu ms, sleeping for %lu s", (unsigned long)millis(),
        (unsigned long)(sleepUs / 1000000));

    // Make sure the log line gets out before the power goes
    Serial.flush();

    // This never returns. The chip powers down, and the next thing that happens is a fresh
    // boot when the timer fires.
    ESP.deepSleep(sleepUs);
}
#endif

//==============================================================================
// Public

// Called from setup() in main.ino, once per boot
void appSetup(void)
{
    // The speed has to match the serial monitor's, which `make monitor` sets to 115200
    Serial.begin(115200);

    // The chip prints its own boot messages at a different speed, which shows up as
    // garbage. This starts our output on a fresh line.
    Serial.println();

    // A cold boot is anything other than waking from deep sleep: power on, the reset
    // button, or flashing. That's when the sensor needs a moment to start up, and when
    // Home Assistant should be told about us again.
    //
    // -> follows a pointer to the struct it points at, and reads a field from it
    bool coldBoot = ESP.getResetInfoPtr()->reason != REASON_DEEP_SLEEP_AWAKE;

    // .c_str() turns Arduino's String into the plain char pointer printf wants.
    // The ? : picks one of two values: "on" if DEEP_SLEEP is non-zero, "off" otherwise.
    LOG("APP", "Starting (%s), deep sleep is %s", ESP.getResetReason().c_str(), DEEP_SLEEP ? "on" : "off");

    // 1. Settings. Start from the defaults, and let the saved settings replace them if
    // there are any. If there aren't, the defaults stay.
    settings = defaultSettings();
    if (!loadSettings(&settings)) {
        LOG("APP", "No saved settings, using the defaults");
    }
    LOG("APP", "Interval %u min, temperature offset %.1f °C, humidity offset %.1f %%",
        settings.intervalMinutes, settings.temperatureOffset, settings.humidityOffset);

    // RTC memory survives deep sleep but not a power cut. After a cold boot, Home Assistant
    // might have restarted or never heard of us, so announce again to be safe.
    loadRtcState(&rtc);
    if (coldBoot) {
        rtc.announced = false;
    }

    readingStartedMs = millis();
    takeReading(coldBoot);

    // 7. With deep sleep, this is the end. The next reading starts from a fresh boot.
#if DEEP_SLEEP
    sleepUntilNextReading(settings.intervalMinutes);
#else
    LOG("APP", "Next reading in %u min", settings.intervalMinutes);
#endif
}

// Called from loop() in main.ino, over and over, forever. With DEEP_SLEEP on it's never
// reached, so it compiles to an empty function.
void appLoop(void)
{
#if !DEEP_SLEEP
    // 7. Without deep sleep, wait here instead. Counting from when the last reading
    // started keeps them evenly spaced. The subtraction gives the right answer even when
    // millis() wraps back to 0, which it does every 49 days.
    //
    // The interval is read fresh each time, so a new interval from Home Assistant is used
    // straight away.
    uint32_t intervalMs = (uint32_t)settings.intervalMinutes * 60 * 1000;

    if (millis() - readingStartedMs >= intervalMs) {
        readingStartedMs = millis();
        takeReading(false);
        LOG("APP", "Next reading in %u min", settings.intervalMinutes);
    }

    // delay() also gives the wifi stack time to do its own work
    delay(100);
#endif
}
