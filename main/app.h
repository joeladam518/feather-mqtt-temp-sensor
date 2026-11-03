// app.h: what the device does for each reading.
//
// A header file (.h) lists what a .cpp file offers to other files, without the code itself.
// main.ino includes this, so it knows these two functions exist and how to call them. The
// code that does the work is in app.cpp.

// #pragma once stops this file being read twice if it's included twice, which would
// otherwise cause "already declared" errors
#pragma once

// Called once at startup. Takes the first reading. With DEEP_SLEEP on, it then sleeps,
// and never returns.
//
// (void) means it takes no arguments. In C++ an empty () means the same thing, but (void)
// says it outright.
void appSetup(void);

// Called over and over after appSetup. With DEEP_SLEEP off, this is where it waits for the
// next reading. With it on, it's never reached.
void appLoop(void);
