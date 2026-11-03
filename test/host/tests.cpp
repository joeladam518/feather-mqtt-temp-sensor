// Tests the logic that doesn't need a board. Run with `make test` or scripts/test-host.sh
//
// There's no test framework, just the CHECK macro below. A test named in main() at the
// bottom is what makes it run, so a new test has to be added there.
//
// Only settings.cpp and payloads.cpp are tested here, because they're the only parts with
// no hardware code in them. The wifi, sensor and storage code needs a real board, and gets
// checked by flashing it and watching the serial log.
//
// Each test is a function named after what it checks, so a failure message reads like a
// sentence: "FAILED test_sleep_has_a_minimum:..." tells you what broke before you open
// the file.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#include "config.h"
#include "payloads.h"
#include "settings.h"
#include "topics.h"

// Counts instead of stopping, so that one run shows every failure at once
static int failures = 0;
static int checks = 0;

// A macro instead of a function, because it needs #condition (the code itself, as text),
// __func__, and __LINE__ to print exactly where a failure happened. The do/while(0) is
// the standard way to make a multi line macro act like a single statement.
#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        failures++; \
        printf("FAILED %s:%d: %s\n", __func__, __LINE__, #condition); \
    } \
} while (0)

//==============================================================================
// Helpers

// Floats can't hold most decimals exactly. -1.2 is really -1.2000000476837158, so
// comparing with == against a calculated value can fail by a hair. This allows for that.
static bool near(float a, float b)
{
    return fabsf(a - b) < 0.0001f;
}

// True if the text is accepted as a number, and it's the expected number
static bool parses(const char *text, float expected)
{
    float value = 0;
    return parseNumber(text, strlen(text), &value) && near(value, expected);
}

// True if the text is refused, as it should be for anything that isn't a plain number
static bool rejects(const char *text)
{
    float value = 0;
    return !parseNumber(text, strlen(text), &value);
}

// Builds the discovery message and parses it back, so tests can look inside. Checking the
// parsed JSON field by field is sturdier than comparing the whole text, which would break
// on any harmless change in key order.
static JsonDocument discovery(uint16_t intervalMinutes)
{
    static char buffer[DISCOVERY_PAYLOAD_MAX];
    JsonDocument doc;

    size_t length = serializeDiscovery(intervalMinutes, buffer, sizeof(buffer));
    CHECK(length > 0);
    CHECK(deserializeJson(doc, buffer, length) == DeserializationError::Ok);

    return doc;
}

//==============================================================================
// Settings

static void test_defaults_are_five_minutes_and_no_offset(void)
{
    Settings_t settings = defaultSettings();
    CHECK(settings.intervalMinutes == 5);
    CHECK(settings.temperatureOffset == 0.0f);
    CHECK(settings.humidityOffset == 0.0f);
    CHECK(settingsValid(&settings));
}

// What Home Assistant's number entities send, and what you'd type into mosquitto_pub
static void test_parses_numbers(void)
{
    CHECK(parses("5", 5));
    CHECK(parses("5.0", 5));
    CHECK(parses("-1.2", -1.2f));
    CHECK(parses("+3", 3));
    CHECK(parses(" 3", 3));
    CHECK(parses("3\n", 3));
}

static void test_rejects_things_that_are_not_numbers(void)
{
    CHECK(rejects(""));
    CHECK(rejects("abc"));
    CHECK(rejects("5min"));
    CHECK(rejects("1.2.3"));
    CHECK(rejects("nan"));
    CHECK(rejects("inf"));
    CHECK(rejects("1e999")); // Too big for a float, so it turns into infinity
    CHECK(rejects("1234567890123456"));
}

// MQTT payloads aren't NUL terminated, so only length characters may be read
static void test_parse_stops_at_the_length(void)
{
    float value = 0;
    CHECK(parseNumber("12345", 2, &value) && near(value, 12));
}

static void test_interval_is_clamped_and_rounded(void)
{
    Settings_t settings = defaultSettings();

    CHECK(applySetting(&settings, SETTING_INTERVAL, 10));
    CHECK(settings.intervalMinutes == 10);

    CHECK(applySetting(&settings, SETTING_INTERVAL, 0));
    CHECK(settings.intervalMinutes == INTERVAL_MIN_MINUTES);

    // Already at the minimum, so clamping to it again isn't a change
    CHECK(!applySetting(&settings, SETTING_INTERVAL, -30));
    CHECK(settings.intervalMinutes == INTERVAL_MIN_MINUTES);

    CHECK(applySetting(&settings, SETTING_INTERVAL, 1000));
    CHECK(settings.intervalMinutes == INTERVAL_MAX_MINUTES);

    CHECK(applySetting(&settings, SETTING_INTERVAL, 4.6f));
    CHECK(settings.intervalMinutes == 5);
}

static void test_offsets_are_clamped_and_rounded_to_a_tenth(void)
{
    Settings_t settings = defaultSettings();

    CHECK(applySetting(&settings, SETTING_TEMPERATURE_OFFSET, -1.234f));
    CHECK(near(settings.temperatureOffset, -1.2f));

    CHECK(applySetting(&settings, SETTING_TEMPERATURE_OFFSET, 50));
    CHECK(near(settings.temperatureOffset, TEMPERATURE_OFFSET_MAX));

    CHECK(applySetting(&settings, SETTING_HUMIDITY_OFFSET, 3.06f));
    CHECK(near(settings.humidityOffset, 3.1f));

    CHECK(applySetting(&settings, SETTING_HUMIDITY_OFFSET, -50));
    CHECK(near(settings.humidityOffset, HUMIDITY_OFFSET_MIN));
}

// A command that doesn't change anything mustn't cause a flash write
static void test_same_value_is_not_a_change(void)
{
    Settings_t settings = defaultSettings();

    CHECK(!applySetting(&settings, SETTING_INTERVAL, 5));
    CHECK(!applySetting(&settings, SETTING_INTERVAL, 5.2f));
    CHECK(!applySetting(&settings, SETTING_TEMPERATURE_OFFSET, 0.04f));

    CHECK(applySetting(&settings, SETTING_TEMPERATURE_OFFSET, -0.7f));
    CHECK(!applySetting(&settings, SETTING_TEMPERATURE_OFFSET, -0.7f));
}

// Whatever applySetting produces has to pass the check used on settings read from flash
static void test_applied_settings_are_always_valid(void)
{
    const float values[] = {-1000, -20, -10.05f, -0.05f, 0, 0.05f, 9.99f, 60.4f, 1000};

    for (float value : values) {
        Settings_t settings = defaultSettings();
        applySetting(&settings, SETTING_INTERVAL, value);
        applySetting(&settings, SETTING_TEMPERATURE_OFFSET, value);
        applySetting(&settings, SETTING_HUMIDITY_OFFSET, value);
        CHECK(settingsValid(&settings));
    }
}

static void test_rejects_corrupt_settings(void)
{
    Settings_t settings = defaultSettings();
    settings.intervalMinutes = 0;
    CHECK(!settingsValid(&settings));

    settings = defaultSettings();
    settings.intervalMinutes = 61;
    CHECK(!settingsValid(&settings));

    settings = defaultSettings();
    settings.temperatureOffset = NAN;
    CHECK(!settingsValid(&settings));

    settings = defaultSettings();
    settings.humidityOffset = 25;
    CHECK(!settingsValid(&settings));
}

//==============================================================================
// Calibration and timing

static void test_calibration_adds_the_offsets(void)
{
    Settings_t settings = defaultSettings();
    settings.temperatureOffset = -1.5f;
    settings.humidityOffset = 3.0f;

    float temperature = 0;
    float humidity = 0;
    applyCalibration(&settings, 22.0f, 40.0f, &temperature, &humidity);

    CHECK(near(temperature, 20.5f));
    CHECK(near(humidity, 43.0f));
}

static void test_calibrated_humidity_stays_between_0_and_100(void)
{
    Settings_t settings = defaultSettings();
    float temperature = 0;
    float humidity = 0;

    settings.humidityOffset = 10;
    applyCalibration(&settings, 20, 95, &temperature, &humidity);
    CHECK(humidity == 100.0f);

    settings.humidityOffset = -10;
    applyCalibration(&settings, 20, 5, &temperature, &humidity);
    CHECK(humidity == 0.0f);
}

// The time spent awake comes off the sleep, so readings stay evenly spaced
static void test_sleep_subtracts_time_awake(void)
{
    CHECK(sleepMicroseconds(5, 0) == 300000000ULL);
    CHECK(sleepMicroseconds(5, 3000) == 297000000ULL);
}

static void test_sleep_has_a_minimum(void)
{
    CHECK(sleepMicroseconds(1, 59000) == (uint64_t)MIN_SLEEP_MS * 1000);
    CHECK(sleepMicroseconds(1, 120000) == (uint64_t)MIN_SLEEP_MS * 1000);
}

// An hour in microseconds is 3.6 billion, which is close to the 4.29 billion a uint32_t
// can hold. This would catch the math being done in 32 bits by mistake.
static void test_longest_sleep_does_not_overflow(void)
{
    CHECK(sleepMicroseconds(INTERVAL_MAX_MINUTES, 0) == 3600000000ULL);
}

static void test_expire_after_allows_two_missed_readings(void)
{
    CHECK(expireAfterSeconds(5) == 660);
    CHECK(expireAfterSeconds(1) == 180);
}

//==============================================================================
// Payloads

static void test_state_is_rounded_to_a_tenth(void)
{
    char buffer[STATE_PAYLOAD_MAX];
    size_t length = serializeState(21.43f, 45.06f, -61, buffer, sizeof(buffer));

    CHECK(length > 0);
    CHECK(strcmp(buffer, "{\"temperature\":21.4,\"humidity\":45.1,\"rssi\":-61}") == 0);
}

static void test_state_handles_below_freezing(void)
{
    char buffer[STATE_PAYLOAD_MAX];
    CHECK(serializeState(-12.36f, 80.0f, -70, buffer, sizeof(buffer)) > 0);
    CHECK(strstr(buffer, "\"temperature\":-12.4") != NULL);
    CHECK(strstr(buffer, "\"humidity\":80") != NULL);
}

// NaN would make invalid JSON, so a failed reading is never sent
static void test_state_refuses_a_failed_reading(void)
{
    char buffer[STATE_PAYLOAD_MAX];
    CHECK(serializeState(NAN, 45.0f, -61, buffer, sizeof(buffer)) == 0);
    CHECK(serializeState(21.0f, NAN, -61, buffer, sizeof(buffer)) == 0);
}

static void test_payloads_refuse_a_small_buffer(void)
{
    char buffer[16];
    Settings_t settings = defaultSettings();

    CHECK(serializeState(21.0f, 45.0f, -61, buffer, sizeof(buffer)) == 0);
    CHECK(serializeSettings(&settings, buffer, sizeof(buffer)) == 0);
    CHECK(serializeDiscovery(5, buffer, sizeof(buffer)) == 0);
}

static void test_settings_payload(void)
{
    Settings_t settings = defaultSettings();
    settings.intervalMinutes = 10;
    settings.temperatureOffset = -1.2f;
    settings.humidityOffset = 3.0f;

    char buffer[SETTINGS_PAYLOAD_MAX];
    CHECK(serializeSettings(&settings, buffer, sizeof(buffer)) > 0);
    CHECK(strcmp(buffer, "{\"interval\":10,\"temperature_offset\":-1.2,\"humidity_offset\":3}") == 0);
}

// Home Assistant's value_templates have to point at keys that are really there
static void test_value_templates_match_the_payloads(void)
{
    JsonDocument doc = discovery(5);
    JsonObject components = doc["cmps"];

    CHECK(strcmp(components["temperature"]["value_template"], "{{ value_json.temperature }}") == 0);
    CHECK(strcmp(components["humidity"]["value_template"], "{{ value_json.humidity }}") == 0);
    CHECK(strcmp(components["rssi"]["value_template"], "{{ value_json.rssi }}") == 0);
    CHECK(strcmp(components["interval"]["value_template"], "{{ value_json.interval }}") == 0);
    CHECK(strcmp(components["temperature_offset"]["value_template"], "{{ value_json.temperature_offset }}") == 0);
    CHECK(strcmp(components["humidity_offset"]["value_template"], "{{ value_json.humidity_offset }}") == 0);

    char buffer[SETTINGS_PAYLOAD_MAX];
    Settings_t settings = defaultSettings();
    serializeSettings(&settings, buffer, sizeof(buffer));

    JsonDocument settingsDoc;
    deserializeJson(settingsDoc, buffer);
    CHECK(settingsDoc["interval"].is<int>());
    CHECK(settingsDoc["temperature_offset"].is<float>());
    CHECK(settingsDoc["humidity_offset"].is<float>());
}

static void test_discovery_describes_the_device(void)
{
    JsonDocument doc = discovery(5);

    CHECK(strcmp(doc["dev"]["ids"], DEVICE_ID) == 0);
    CHECK(strcmp(doc["dev"]["name"], DEVICE_NAME) == 0);
    CHECK(doc["o"]["name"].is<const char *>());
    CHECK(doc["cmps"].size() == 6);

    JsonObject temperature = doc["cmps"]["temperature"];
    CHECK(strcmp(temperature["p"], "sensor") == 0);
    CHECK(strcmp(temperature["device_class"], "temperature") == 0);
    CHECK(strcmp(temperature["unit_of_measurement"], "°C") == 0);
    CHECK(strcmp(temperature["state_topic"], TOPIC_STATE) == 0);
    CHECK(strcmp(temperature["unique_id"], DEVICE_ID "_temperature") == 0);
}

// A sleeping device can't have an availability topic, so readings expire instead
static void test_discovery_expiry_follows_the_interval(void)
{
    JsonDocument doc = discovery(10);

    CHECK(doc["cmps"]["temperature"]["expire_after"] == expireAfterSeconds(10));
    CHECK(doc["cmps"]["humidity"]["expire_after"] == expireAfterSeconds(10));
    CHECK(doc["cmps"]["temperature"]["availability_topic"].isNull());
}

// Retain is what lets a command wait on the broker until the device wakes up
static void test_settings_are_retained_numbers(void)
{
    JsonDocument doc = discovery(5);
    const char *keys[] = {"interval", "temperature_offset", "humidity_offset"};
    const char *topics[] = {TOPIC_INTERVAL_SET, TOPIC_TEMPERATURE_OFFSET_SET, TOPIC_HUMIDITY_OFFSET_SET};

    for (int i = 0; i < 3; i++) {
        JsonObject number = doc["cmps"][keys[i]];
        CHECK(strcmp(number["p"], "number") == 0);
        CHECK(number["retain"] == true);
        CHECK(strcmp(number["command_topic"], topics[i]) == 0);
        CHECK(strcmp(number["state_topic"], TOPIC_SETTINGS) == 0);
        CHECK(strcmp(number["entity_category"], "config") == 0);
    }
}

// Home Assistant's number boxes have to allow the same range the device does
static void test_discovery_limits_match_the_settings(void)
{
    JsonDocument doc = discovery(5);
    JsonObject components = doc["cmps"];

    CHECK(components["interval"]["min"] == INTERVAL_MIN_MINUTES);
    CHECK(components["interval"]["max"] == INTERVAL_MAX_MINUTES);
    CHECK(components["temperature_offset"]["min"] == TEMPERATURE_OFFSET_MIN);
    CHECK(components["temperature_offset"]["max"] == TEMPERATURE_OFFSET_MAX);
    CHECK(components["humidity_offset"]["min"] == HUMIDITY_OFFSET_MIN);
    CHECK(components["humidity_offset"]["max"] == HUMIDITY_OFFSET_MAX);
}

// With a device_class of temperature, Home Assistant would convert an offset of 0 °C to 32 °F
static void test_temperature_offset_is_not_converted(void)
{
    JsonDocument doc = discovery(5);
    CHECK(doc["cmps"]["temperature_offset"]["device_class"].isNull());
}

// Prints how much of the buffer discovery uses, to show how much room is left
static void test_discovery_fits_with_room_to_spare(void)
{
    static char buffer[DISCOVERY_PAYLOAD_MAX];
    size_t length = serializeDiscovery(INTERVAL_MAX_MINUTES, buffer, sizeof(buffer));

    printf("Discovery is %zu of %d bytes\n", length, DISCOVERY_PAYLOAD_MAX);
    CHECK(length > 0);
    CHECK(length < DISCOVERY_PAYLOAD_MAX * 3 / 4);
}

static void test_topics_match_exactly(void)
{
    const char *topic = TOPIC_INTERVAL_SET;
    CHECK(topicEquals(TOPIC_INTERVAL_SET, topic, strlen(topic)));
    CHECK(!topicEquals(TOPIC_INTERVAL_SET, topic, strlen(topic) - 1));
    CHECK(!topicEquals(TOPIC_INTERVAL_SET, "", 0));
    CHECK(strcmp(HA_DISCOVERY_TOPIC, "homeassistant/device/test_sensor/config") == 0);
}

// Runs every test, then prints a summary. Returning 1 makes the shell, and so `make test`,
// report a failure.
int main(void)
{
    test_defaults_are_five_minutes_and_no_offset();
    test_parses_numbers();
    test_rejects_things_that_are_not_numbers();
    test_parse_stops_at_the_length();
    test_interval_is_clamped_and_rounded();
    test_offsets_are_clamped_and_rounded_to_a_tenth();
    test_same_value_is_not_a_change();
    test_applied_settings_are_always_valid();
    test_rejects_corrupt_settings();
    test_calibration_adds_the_offsets();
    test_calibrated_humidity_stays_between_0_and_100();
    test_sleep_subtracts_time_awake();
    test_sleep_has_a_minimum();
    test_longest_sleep_does_not_overflow();
    test_expire_after_allows_two_missed_readings();
    test_state_is_rounded_to_a_tenth();
    test_state_handles_below_freezing();
    test_state_refuses_a_failed_reading();
    test_payloads_refuse_a_small_buffer();
    test_settings_payload();
    test_value_templates_match_the_payloads();
    test_discovery_describes_the_device();
    test_discovery_expiry_follows_the_interval();
    test_settings_are_retained_numbers();
    test_discovery_limits_match_the_settings();
    test_temperature_offset_is_not_converted();
    test_discovery_fits_with_room_to_spare();
    test_topics_match_exactly();

    printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
