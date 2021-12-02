/*
 * LoRa-trans (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

#ifndef __LORAL2_H
#define __LORAL2_H

#include <cstddef>
#include <cinttypes>

class LoRaL2Packet {
public:
	LoRaL2Packet(const LoRaL2Packet&) = delete;
	void operator=(const LoRaL2Packet&) = delete;
	LoRaL2Packet() = delete;

	LoRaL2Packet(uint8_t *packet, int len, int rssi, int err);
	virtual ~LoRaL2Packet();
	
	uint8_t *packet;
	int len;
	int rssi;
	int err;
};

typedef void (*recv_callback)(LoRaL2Packet*);

class LoRaL2 {
public:
	LoRaL2(const LoRaL2&) = delete;
	void operator=(const LoRaL2&) = delete;
	
	LoRaL2(long int band, int spread, int bandwidth,
		const char *key, int key_len, recv_callback recv_cb);
	virtual ~LoRaL2();
	
	bool send(const uint8_t *packet, int payload_len);
	uint32_t speed_bps() const;
	int max_payload() const;
	bool ok() const;
	// public because LoRa C API needs to call them
	void on_recv(int rssi, uint8_t* packet, int len);
	void on_sent();

	/* private */
	void resume_rx();
	uint8_t *encrypt(const uint8_t *packet, int len, int& new_len);
	uint8_t *append_fec(const uint8_t *packet, int len, int& new_len);
	uint8_t *decode_fec(const uint8_t *packet, int len, int& new_len, int& err);
	uint8_t *decrypt(const uint8_t *packet, int len, int& new_len, int& err);
	static uint8_t *hashed_key(const char* key, size_t len);
	static void gen_iv(uint8_t* buffer, int len);

	long int band;
	int spread;
	int bandwidth;
	uint8_t *hkey;
	recv_callback recv_cb;
	int status;
	bool _ok;
};

#endif
