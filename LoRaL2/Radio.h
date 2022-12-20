/*
 * LoRaL2 (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

#ifndef __RADIO_H
#define __RADIO_H

#include <cinttypes>
#include <cstddef>

class LoRaL2;

bool lora_start(long int band, int spread, int bandwidth, int power,
		int paboost, int cr4slsh, LoRaL2 *pobserver);
void lora_receive();
bool lora_begin_packet();
void lora_finish_packet(const uint8_t* packet, size_t len);

#endif
