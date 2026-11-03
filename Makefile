# Shortcuts for arduino-cli. Run `make help` to see them.
#
# The board and library versions come from main/sketch.yaml. The serial port comes from
# PORT, or from ESPPORT if you've exported that, so either of these works:
#
#   make flash PORT=/dev/cu.usbserial-0001
#   export ESPPORT=/dev/cu.usbserial-0001; make flash
#
# A quick Makefile primer: each rule is a target name, a colon, and the targets it needs
# first. The indented lines under it (they must be tabs, not spaces) are the commands it
# runs. `make flash` runs `build` and `check-port` first, then its own command. A line that
# starts with @ runs without being printed first.

# := sets a variable once. ?= only sets it if it isn't set already, which is what lets
# PORT come from the command line or from ESPPORT.
SKETCH := main
BUILD  := build
PORT   ?= $(ESPPORT)
BAUD   := 115200

# .PHONY says these are command names, not files. Without it, a file called "build" in
# this folder would make `make build` think there was nothing to do.
.PHONY: help build flash monitor flash-monitor test clean check-config check-port

# The first target is what plain `make` runs, so `make` on its own shows this list
help:
	@echo "make build          Compile the firmware"
	@echo "make flash          Compile, then write it to the board"
	@echo "make monitor        Show the board's serial output (quit with Ctrl+C)"
	@echo "make flash-monitor  Both of the above"
	@echo "make test           Run the tests on this computer, no board needed"
	@echo "make clean          Delete the build output"

# --profile huzzah picks the board and pinned versions from main/sketch.yaml.
# --build-path keeps the output in build/ here, instead of a hidden temporary folder,
# so `make flash` can find it.
build: check-config
	arduino-cli compile --profile huzzah --build-path $(BUILD) --warnings default $(SKETCH)

# --input-dir flashes what `build` just made, instead of compiling again
flash: build check-port
	arduino-cli upload --profile huzzah --input-dir $(BUILD) --port $(PORT) $(SKETCH)

# The baud rate has to match Serial.begin() in app.cpp
monitor: check-port
	arduino-cli monitor --port $(PORT) --config baudrate=$(BAUD)

flash-monitor: flash monitor

test:
	scripts/test-host.sh

clean:
	rm -rf $(BUILD)

# The build fails with a confusing "config.h: No such file" deep in the compiler output
# without this, so check for it up front and say what to do
check-config:
	@test -f $(SKETCH)/config.h || { \
		echo "main/config.h is missing. Create it with: cp main/config_example.h main/config.h" >&2; \
		exit 1; \
	}

# Same idea for the serial port. $(MAKECMDGOALS) is whatever target you ran, so the hint
# shows your own command back to you.
check-port:
	@test -n "$(PORT)" || { \
		echo "No serial port. Find it with: ls /dev/cu.*" >&2; \
		echo "Then run: make $(MAKECMDGOALS) PORT=/dev/cu.usbserial-xxxx" >&2; \
		exit 1; \
	}
