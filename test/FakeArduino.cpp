/*
 * LoRa-trans (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

// Emulation of certain Arduino APIs for testing on UNIX

#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Emulation of millis() and random()

static struct timeval tm_first;
static bool virgin = true;

static void init_things()
{
	virgin = false;
	gettimeofday(&tm_first, 0);
	srandom(tm_first.tv_sec + tm_first.tv_usec);
}

uint32_t arduino_millis()
{
	if (virgin) init_things();
	struct timeval tm;
	gettimeofday(&tm, 0);
	int64_t now_us   = tm.tv_sec       * 1000000LL + tm.tv_usec;
	int64_t start_us = tm_first.tv_sec * 1000000LL + tm_first.tv_usec;
	// uptime in ms
	int64_t uptime_ms = (now_us - start_us) / 1000 + 1;
	return (uint32_t) (uptime_ms & 0xffffffffULL);
}

int32_t arduino_random(int32_t min, int32_t max)
{
	if (virgin) init_things();
	return min + random() % (max - min);
}

class LoRaL2;

bool lora_start(long int band, int spread, int bandwidth, int power,
		int paboost, int cr4slsh, LoRaL2 *pobserver)
{
	return true;
}

void lora_receive()
{
}

bool lora_begin_packet()
{
	return true;
}

bool lora_finish_packet(const uint8_t* packet, int len)
{
}
