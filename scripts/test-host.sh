#!/usr/bin/env bash
# Builds and runs the tests on this computer, no board needed.
#
# This works because the settings rules and the payloads have no hardware code in them.
# They're ordinary C++, so a normal compiler can build them.

# Stop at the first error, including errors in the middle of a pipeline
set -Eeo pipefail

# The project root, worked out from where this script is, so it runs from any directory
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# ArduinoJson comes from the same copy that the firmware uses, which arduino-cli downloads
# into its own cache on the first build. The version is read from sketch.yaml.
#
# sed pulls the version out of the line "- ArduinoJson (7.4.3)". -n prints nothing by default,
# and the p at the end prints only the line that matched, with the version swapped in.
version="$(sed -n 's/.*- ArduinoJson (\(.*\))/\1/p' "$root/main/sketch.yaml")"

# Where arduino-cli keeps its downloads. The || fallback is the usual place on a Mac, in
# case the config command fails.
data="$(arduino-cli config get directories.data 2>/dev/null || echo "$HOME/Library/Arduino15")"

# Profile downloads go in internal/, in a folder named after the library, its version, and
# a hash. The * matches the hash.
json="$(ls -d "$data"/internal/ArduinoJson_"$version"_*/ArduinoJson/src 2>/dev/null | head -n 1)"

if [ -z "$json" ]; then
    echo "ArduinoJson $version hasn't been downloaded yet. Run: make build" >&2
    exit 1
fi

# Build in a temporary directory, and delete it when this script ends, however it ends
output="$(mktemp -d)"
trap 'rm -rf "$output"' EXIT

# Copy the code being tested, without your real config.h, so the test one gets used instead.
# The .cpp files have to come too: #include "config.h" looks in the .cpp file's own folder
# first, and in main/ that would find your real config.h.
mkdir "$output/main"
cp "$root"/main/*.h "$root/main/settings.cpp" "$root/main/payloads.cpp" "$output/main/"
rm -f "$output/main/config.h"

# -fsanitize adds run time checks that fail the test on memory bugs (like reading past the
# end of a buffer) and undefined behaviour, which might otherwise go unnoticed.
# -isystem hides warnings that come from inside ArduinoJson.
c++ -std=c++17 -Wall -Wextra -g \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -I"$root/test/host" -I"$output/main" -isystem "$json" \
    "$root/test/host/tests.cpp" \
    "$output/main/settings.cpp" \
    "$output/main/payloads.cpp" \
    -o "$output/tests"

# Run them. Its exit code becomes this script's, because it's the last command, so
# `make test` fails when a test does.
"$output/tests"
