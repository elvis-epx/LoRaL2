/*
 * LoRaL2 (LoRa layer-2) project
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
#include "LoRaL2.h"

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

uint8_t* lora_test_last_sent = 0;
size_t lora_test_last_sent_len = 0;

// Emulation of LoRa APIs, network and radio

#define PORT 6000
#define GROUP "239.0.0.1"

static int sock = -1;
static int coverage = 0;

int lora_emu_socket()
{
	return sock;
}

// simulate different coverage areas
// e.g. packet sent to coverage 0x01 is seen by stations
// with coverage 0x03 but not by stations with cov. 0x02
void lora_emu_socket_coverage(int c)
{
	coverage = c;
}

static void setup_lora()
{
	// from https://web.cs.wpi.edu/~claypool/courses/4514-B99/samples/multicast.c
	struct ip_mreq mreq;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("fake: socket");
		exit(1);
	}

	// Self-receive must be enabled because we run multiple instances
	// on the same machine
	int loop = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
		perror("fake: setsockopt loop");
		exit(1);
	}

	// Allow multiple listeners to the same port
	int optval = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		perror("fake: setsockopt reuseport");
		exit(1);
	}

	// Enter multicast group
	mreq.imr_multiaddr.s_addr = inet_addr(GROUP);
	mreq.imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
					&mreq, sizeof(mreq)) < 0) {
		perror("fake: setsockopt mreq");
		exit(1);
	}

	// listen UDP port
	struct sockaddr_in addr;
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PORT);

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("fake: bind");
		exit(1);
	}
}

LoRaL2* observer;

bool lora_start(long int band, int spread, int bandwidth, int power,
		int paboost, int cr4slsh, LoRaL2 *pobserver)
{
	setup_lora();
	observer = pobserver;
	return true;
}

void lora_receive()
{
}

bool lora_emu_sim_senderr = true;

bool lora_begin_packet()
{
	if (lora_emu_sim_senderr) {
		return arduino_random(0, 100) > 0;
	}
	return true;
}

bool lora_emu_call_onsent = true;

void lora_finish_packet(const uint8_t* packet, size_t len)
{
	if (lora_test_last_sent) {
		free(lora_test_last_sent);
		lora_test_last_sent = 0;
	}
	lora_test_last_sent = (uint8_t*) malloc(len);
	memcpy(lora_test_last_sent, packet, len);
	lora_test_last_sent_len = len;

	// Send to multicast group & port
	struct sockaddr_in addr;
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(GROUP);
	addr.sin_port = htons(PORT);

	// add coverage bitmask
	uint8_t* c = (uint8_t*) malloc(len + 1);
	c[0] = coverage;
	memcpy(c + 1, packet, len);

#ifdef LORA_EMU_DUMP
	printf("fake: lora_emu_tx ");
	for(size_t i = 0; i < (len + 1); ++i) {
		printf("%d ", c[i]);
	}
	printf("\n");
#endif

	int sent = sendto(sock, c, len + 1, 0, (struct sockaddr *) &addr, sizeof(addr));
	free(c);

	if (sent < 0) {
		perror("fake: sendto");
		exit(1);
	}
	// printf("fake: Sent packet\n");

	if (lora_emu_call_onsent) {
		observer->on_sent();
	}
}

void lora_emu_rx()
{
	char rawmsg[257];
	char *msg = rawmsg + 1;
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	int len = recvfrom(sock, rawmsg, sizeof(rawmsg), 0, (struct sockaddr *) &from, &fromlen);
	if (len < 0) {
		perror("fake: recvfrom");
		exit(1);
	}
	printf("fake: Packet received, coverage %d\n", rawmsg[0]);
	if (! (coverage & rawmsg[0])) {
		printf("fake: Discarding packet out of simulated coverage.\n");
		return;
	}

	len -= 1;
	if (arduino_random(0, 3) == 0) {
		printf("fake: Received packet, corrupt it a little\n");
		for (int i = 0; i < 3; ++i) {
			msg[arduino_random(0, len)] = arduino_random(0, 256);
		}
	} else if (arduino_random(0, 100) == 0) {
		printf("fake: Received packet, corrupt it a lot\n");
		for (int i = 0; i < 30; ++i) {
			msg[arduino_random(0, len)] = arduino_random(0, 256);
		}
	} else {
		printf("fake: Received packet, not corrupting\n");
	}

	uint8_t* bmsg = (uint8_t*) malloc(len);
	memcpy(bmsg, msg, len);

#ifdef LORA_EMU_DUMP
	printf("fake: lora_emu_rx ");
	for(int i = 0; i < len; ++i) {
		printf("%d ", bmsg[i]);
	}
	printf("\n");
#endif

	observer->on_recv(-50, bmsg, len);
}
