/*
 * LoRaL2 (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

#include <Arduino.h>

uint32_t arduino_millis()
{
	return millis();
}

int32_t arduino_random(int32_t min, int32_t max)
{
	return random(min, max);
}
