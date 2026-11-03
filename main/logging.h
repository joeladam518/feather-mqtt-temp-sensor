// logging.h: prints to the serial monitor, with a tag saying which part of the code it's from.
//
//   LOG("WIFI", "Connected in %lu ms", elapsed);   prints   [WIFI] Connected in 812 ms
//
// It's a macro, not a function, so the "\n" can be glued onto your format string at compile
// time. ##__VA_ARGS__ is how a macro passes along any extra arguments, and the ## drops the
// comma before it when there aren't any.
//
// The format works like printf: %d for whole numbers, %u for unsigned ones, %lu for
// unsigned long, %.1f for a float with one decimal, %s for text, and %% for a % sign.
//
// The tag has to be a string in quotes, like "WIFI", because it gets joined onto the
// format string by the compiler.

#pragma once

// For Serial
#include <Arduino.h>

#define LOG(tag, format, ...) Serial.printf("[" tag "] " format "\n", ##__VA_ARGS__)
