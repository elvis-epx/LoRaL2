/*
 * LoRaL2 (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

// Platform-dependent functions. They are faked on Linux to run tests.

#ifndef __ARDUINO_BRIDGE
#define __ARDUINO_BRIDGE

#include <cstdint>

uint32_t arduino_millis();
int32_t arduino_random(int32_t min, int32_t max);

#endif
