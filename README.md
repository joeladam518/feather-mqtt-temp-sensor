# feather-mqtt-temp-sensor

A battery friendly temperature and humidity sensor that shows up in Home Assistant by itself. Every few minutes it
wakes up, reads the sensor, sends the reading over MQTT, and goes back to deep sleep. Deep sleep can be turned off for a
board that's always on USB. The reporting interval and the
calibration offsets can be changed from Home Assistant, and the device remembers them.

Firmware for the [Adafruit Feather HUZZAH ESP8266](https://learn.adafruit.com/adafruit-feather-huzzah-esp8266) with an
AM2301 (DHT21) sensor, built with [arduino-cli](https://arduino.github.io/arduino-cli/).

- [Hardware](#hardware)
- [Setup](#setup)
- [Commands](#commands)
- [How it works](#how-it-works)
- [Home Assistant](#home-assistant)
- [Calibrating](#calibrating)
- [MQTT reference](#mqtt-reference)
- [Debugging](#debugging)
- [Tests](#tests)

## Hardware

| What | Feather pin | Setting in `config.h` |
|---|---|---|
| AM2301 data (yellow) | GPIO 14 | `SENSOR_PIN` |
| AM2301 power (red) | 3V | |
| AM2301 ground (black) | GND | |
| **Wire from GPIO 16 to RST** | 16 → RST | `DEEP_SLEEP` |

**With `DEEP_SLEEP` on, the GPIO 16 to RST wire is required.** Deep sleep ends with GPIO 16 pulsing low, and that wire
turns the pulse into a reset, which is what wakes the board. Without it the board hangs after its first reading, and the
serial monitor shows `ets_main.c` as the last thing it printed. If flashing ever starts failing with the wire in place,
swap it for a 1k resistor, or pull it out while you flash. With `DEEP_SLEEP` set to `0`, the wire isn't needed.

The same firmware runs on USB or a LiPo battery. It can't tell which, so `DEEP_SLEEP` in `config.h` decides: `1` deep
sleeps between readings, which a battery needs, and `0` keeps the board awake, which is fine on USB.

## Setup

Install arduino-cli. You only need to do this once:

```shell
brew install arduino-cli
```

Create your config, then fill in your wifi, MQTT, and device settings. It's gitignored, so your passwords stay local.

```shell
cp main/config_example.h main/config.h
```

Build and flash:

```shell
make build
make flash-monitor PORT=/dev/cu.usbserial-xxxx
```

The first build downloads the ESP8266 board package and the libraries, which takes a few minutes. Their exact versions
are pinned in `main/sketch.yaml`.

## Commands

**Finding the serial port.** Plug in the board and run `ls /dev/cu.*`. Look for something like
`/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`. Set `export ESPPORT=/dev/cu.usbserial-xxxx` and you can leave out
`PORT=` from then on.

| Command | What it does |
|---|---|
| `make build` | Compiles everything into `build/` |
| `make flash` | Builds if needed, then writes to the board |
| `make monitor` | Shows the board's serial output. Quit with `Ctrl+C` |
| `make flash-monitor` | Flashes, then starts the monitor. This is the everyday one |
| `make test` | Runs the tests on this computer, no board needed |
| `make clean` | Deletes the build output. Try this when a build is confused |

**Why main/main.ino?** arduino-cli insists that a sketch's main file is named after its folder. Everything else in
`main/` is ordinary `.cpp` and `.h` files, and they all get compiled.

**Updating a library or the board package.** Change its version in `main/sketch.yaml` and build again.

## How it works

Every reading is a fresh boot. Deep sleep turns off everything except a timer, and when the timer fires, the board
resets and starts from the top. Each time it wakes, the board (`main/app.cpp`):

1. Loads the saved settings, or uses the defaults (`REPORT_INTERVAL_DEFAULT_MS` from `config.h`, and no offsets)
2. Reads the sensor, before wifi is on, because the radio is electrically noisy
3. Connects to wifi and MQTT
4. Collects any settings Home Assistant sent while it was asleep, saves them, and clears them from the broker
5. Sends the discovery message, on the first boot or when a setting has changed
6. Sends the reading
7. Sleeps until the next one. Time spent awake comes off the sleep, so readings stay evenly spaced

With `DEEP_SLEEP` set to `0`, step 7 waits in `loop()` instead of sleeping, and steps 2 to 6 repeat every interval
without a reboot. Everything else is the same, including settings from Home Assistant being picked up at the next
reading, so the two modes behave alike apart from battery life.

Wifi gets 15 seconds and MQTT gets 5. If something fails, the board skips the rest and goes to sleep anyway, rather than
draining the battery trying again. It tries again at the next reading.

To connect faster, the board keeps the access point's channel and address in RTC memory, which stays powered during
deep sleep. That lets it skip the scan, the slowest part of connecting. If the access point has changed, it falls back
to scanning after 5 seconds.

Settings live in flash, and are only written when one actually changes.

## Home Assistant

The device sets itself up through [MQTT discovery](https://www.home-assistant.io/integrations/mqtt/), so there's no YAML
to write. It appears under Settings > Devices & services > MQTT with:

| Entity | Type | Notes |
|---|---|---|
| Temperature | Sensor | Sent in °C. Home Assistant converts it if you've set it to show °F |
| Humidity | Sensor | % |
| Signal strength | Sensor | Wifi RSSI in dBm, under Diagnostic, off until you enable it |
| Report interval | Number | Minutes, 1 to 60. Default from `REPORT_INTERVAL_DEFAULT_MS` in `config.h` |
| Temperature offset | Number | °C, −10 to 10, added to every reading |
| Humidity offset | Number | Percentage points, −20 to 20, added to every reading |

**Changes take effect at the next wake**, which can be up to one interval later. Until then, the number box goes back
to showing the old value. That's expected: it shows what the device is actually using. Home Assistant sends each
change as a retained message, so the broker holds on to it until the device wakes up and collects it.

**There's no online/offline status.** A device that sleeps would show as offline nearly all the time. Instead, readings
go *unavailable* if two in a row are missed, which is two intervals plus a minute.

**The temperature offset is always in °C**, even if Home Assistant shows temperatures in °F. A °F difference is 1.8
times bigger than a °C one, so divide by 1.8: if the sensor reads 2 °F high, set the offset to −1.1.

**Removing the device.** Discovery is retained, so the broker keeps it after the device is gone. Clear it, and the
readings, with:

```shell
mosquitto_pub -h <broker> -r -n -t homeassistant/device/temp_sensor/config
mosquitto_pub -h <broker> -r -n -t temp_sensor/state
mosquitto_pub -h <broker> -r -n -t temp_sensor/settings
```

## Calibrating

1. Put the sensor next to a thermometer or hygrometer you trust, and leave both for half an hour to settle.
2. Work out the difference: reference minus what Home Assistant shows. For example, a reference of 21.0 °C with the
   sensor showing 22.3 °C gives an offset of −1.3.
3. Enter it in the offset box on the device page.

The serial log shows both the raw and the corrected values, so you can check the offset is what you expect.

For humidity, the salt test is a cheap reference: a sealed bag with a damp spoonful of table salt sits at 75% after a
few hours.

## MQTT reference

`temp_sensor` below is `TOPIC_BASE` from `config.h`. Everything the device publishes is retained.

| Topic | Direction | Payload |
|---|---|---|
| `temp_sensor/state` | Out | `{"temperature":21.4,"humidity":45.2,"rssi":-61}` |
| `temp_sensor/settings` | Out | `{"interval":5,"temperature_offset":-1.3,"humidity_offset":0}` |
| `temp_sensor/interval/set` | In | Minutes, e.g. `10` |
| `temp_sensor/temperature_offset/set` | In | °C, e.g. `-1.3` |
| `temp_sensor/humidity_offset/set` | In | Percentage points, e.g. `3` |
| `homeassistant/device/temp_sensor/config` | Out | The discovery message |

The `/set` topics also work without Home Assistant. Send them **retained** (`-r`), or the device will be asleep and miss
them:

```shell
mosquitto_pub -h <broker> -r -t temp_sensor/interval/set -m 1
```

Values outside the limits are clamped to the nearest one, and anything that isn't a number is ignored. Either way the
device clears the command after it reads it, so an old command can't come back and undo a newer change.

## Debugging

**Logs.** Each part logs with a tag: `APP`, `SENSOR`, `WIFI`, `MQTT`, and `STORAGE`. A normal wake looks like:

```
[APP] Starting (Deep-Sleep Wake)
[APP] Interval 5 min, temperature offset -1.3 °C, humidity offset 0.0 %
[APP] Temperature 21.0 °C (raw 22.3), humidity 45.2 % (raw 45.2)
[WIFI] Connected in 640 ms, IP address 192.168.1.50
[MQTT] Connected in 35 ms
[APP] Awake for 2210 ms, sleeping for 297 s
```

The garbage before the first line is the chip's own boot message, printed at a different speed. It's harmless.

**Testing on the desk.** `DEEP_SLEEP 0` keeps the board awake between readings, so you don't need the GPIO 16 wire
while you're working on it.

**Testing without waiting.** Set `REPORT_INTERVAL_DEFAULT_MS` in `config.h` to `60000` (1 minute) while you're working
on it, and back to `300000` (5 minutes) when you're done. It has to be a whole number of minutes, or the build stops
with an error saying so. Flashing with a different default puts the interval back to it, even if an interval was saved
from Home Assistant, while any calibration offsets are kept. You can also change the interval from Home Assistant
without reflashing.

**Watching MQTT.** Often the quickest way to find a problem is to see what's on the broker:

```shell
mosquitto_sub -h <broker> -v -t 'temp_sensor/#' -t 'homeassistant/device/temp_sensor/config'
```

**Common problems.**

| Symptom | Likely cause |
|---|---|
| One reading, then nothing, and the log ends in `ets_main.c` | The GPIO 16 to RST wire is missing or loose. Fix it, or set `DEEP_SLEEP` to `0` |
| `[SENSOR] Reading failed` | Data wire on the wrong pin, or a loose connection |
| `[MQTT] Couldn't connect ... (state -2)` | Wrong broker address or port, or the broker isn't running |
| `[MQTT] Couldn't connect ... (state 4)` or `(state 5)` | Wrong MQTT username or password |
| Flashing fails | Try again. If it keeps failing, see the note about the GPIO 16 wire under [Hardware](#hardware) |

## Tests

```shell
make test
```

This builds the settings rules and the MQTT payloads with your computer's compiler and checks them, with no board
needed. It checks that bad input is rejected, values are clamped, the JSON is valid, and the discovery message matches
what the device actually sends. The ArduinoJson it uses comes from arduino-cli's download, so run `make build` once
first.
