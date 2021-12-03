/*
 * LoRa-trans (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

#include <stdlib.h>
#include "RS-FEC.h"
#include "src/AES.h"
#include "src/sha256.h"
#include "ArduinoBridge.h"
#include "Radio.h"

#define POWER   20
#define PABOOST 1
#define CR4SLSH 5

// FEC-related constants
static const size_t MSGSIZ_SHORT = 50;
static const size_t MSGSIZ_MEDIUM = 100;
static const size_t MSGSIZ_LONG = 200;
static const size_t REDUNDANCY_SHORT = 10;
static const size_t REDUNDANCY_MEDIUM = 14;
static const size_t REDUNDANCY_LONG = 20;

// Crypto-related constant
#define CRYPTO_MAGIC 0x05
#define CRYPTO_LENGTH_LEN  2

#define STATUS_IDLE 0
#define STATUS_RECEIVING 1
#define STATUS_TRANSMITTING 2

#include "LoRaL2.h"

LoRaL2::LoRaL2(long int band, int spread, int bandwidth,
		const char *key, size_t key_len,
		LoRaL2Observer *observer)
{
	this->band = band;
	this->spread = spread;
	this->bandwidth = bandwidth;
	this->hkey = hashed_key(key, key_len);
	this->observer = observer;

	this->status = STATUS_IDLE;

	_ok = lora_start(band, spread, bandwidth, POWER, PABOOST, CR4SLSH, this);
}

LoRaL2::~LoRaL2()
{
	free(hkey);
}

bool LoRaL2::ok() const
{
	return _ok;
}

uint32_t LoRaL2::speed_bps() const
{
	uint32_t bps = bandwidth;
	bps *= spread;
	bps *= 4;
	bps /= CR4SLSH;
	bps /= (1 << spread);
	return bps;
}

void LoRaL2::resume_rx()
{
	if (status != STATUS_RECEIVING) {
		status = STATUS_RECEIVING;
		lora_receive();
	}
}

void LoRaL2::on_sent()
{
	status = STATUS_IDLE;
	resume_rx();
}

void LoRaL2::on_recv(int rssi, uint8_t *buffer, size_t tot_len)
{
	size_t encrypted_len = 0;
	int err = 0;
	uint8_t *encrypted_packet = decode_fec(buffer, tot_len, encrypted_len, err);
	free(buffer);
	
	uint8_t *packet;
	size_t packet_len = 0;
	if (!err) {
		packet = decrypt(encrypted_packet, encrypted_len, packet_len, err);
		free(encrypted_packet);
	} else {
		packet = encrypted_packet;
		packet_len = encrypted_len;
	}

	observer->recv(new LoRaL2Packet(packet, packet_len, rssi, err));
}

bool LoRaL2::send(const uint8_t *packet, size_t payload_len)
{
	if (status == STATUS_TRANSMITTING) {
		return false;
	}

	if (payload_len > max_payload()) {
		return false;
	}

	// should not happen because of 'status' protection
	if (! lora_begin_packet()) return false;

	size_t encrypted_len;
	uint8_t* encrypted_packet = encrypt(packet, payload_len, encrypted_len);

	size_t tot_len;
	uint8_t* fec_packet = append_fec(encrypted_packet, encrypted_len, tot_len);
	free(encrypted_packet);

	status = STATUS_TRANSMITTING;
	lora_finish_packet(fec_packet, tot_len);

	free(fec_packet);

	return true;
}

LoRaL2Packet::LoRaL2Packet(uint8_t *packet, size_t len, int rssi, int err)
{
	this->packet = packet;
	this->len = len;
	this->rssi = rssi;
	this->err = err;
}
	
LoRaL2Packet::~LoRaL2Packet()
{
	free(packet);
}

RS::ReedSolomon<MSGSIZ_SHORT, REDUNDANCY_SHORT> rsf_short;
RS::ReedSolomon<MSGSIZ_MEDIUM, REDUNDANCY_MEDIUM> rsf_medium;
RS::ReedSolomon<MSGSIZ_LONG, REDUNDANCY_LONG> rsf_long;

size_t LoRaL2::max_payload() const
{
	if (hkey) {
		AES256 aes256;
		return MSGSIZ_LONG - aes256.blockSize() * 2 - CRYPTO_LENGTH_LEN;
	}
	return MSGSIZ_LONG;
}

uint8_t *LoRaL2::append_fec(const uint8_t* packet, size_t len, size_t& new_len)
{
	// safety measure, should never happen
	if (len > MSGSIZ_LONG) len = MSGSIZ_LONG;

	new_len = len;

	uint8_t* rs_unencoded    = (uint8_t*) calloc(MSGSIZ_LONG,                   sizeof(char));
	uint8_t* rs_encoded      = (uint8_t*) calloc(MSGSIZ_LONG + REDUNDANCY_LONG, sizeof(char));
	uint8_t* packet_with_fec = (uint8_t*) calloc(len         + REDUNDANCY_LONG, sizeof(char));

	memcpy(rs_unencoded,    packet, len);
	memcpy(packet_with_fec, packet, len);

	if (len <= MSGSIZ_SHORT) {
		rsf_short.Encode(rs_unencoded, rs_encoded);
		new_len += REDUNDANCY_SHORT;
		memcpy(packet_with_fec + len, rs_encoded + MSGSIZ_SHORT, REDUNDANCY_SHORT);
	} else if (len <= MSGSIZ_MEDIUM) {
		rsf_medium.Encode(rs_unencoded, rs_encoded);
		new_len += REDUNDANCY_MEDIUM;
		memcpy(packet_with_fec + len, rs_encoded + MSGSIZ_MEDIUM, REDUNDANCY_MEDIUM);
	} else {
		rsf_long.Encode(rs_unencoded, rs_encoded);
		new_len += REDUNDANCY_LONG;
		memcpy(packet_with_fec + len, rs_encoded + MSGSIZ_LONG, REDUNDANCY_LONG);
	}

	free(rs_encoded);
	free(rs_unencoded);

	return packet_with_fec;
}

uint8_t *LoRaL2::decode_fec(const uint8_t* packet_with_fec, size_t len, size_t& net_len, int& err)
{
	uint8_t* rs_encoded = (uint8_t*) calloc(MSGSIZ_LONG + REDUNDANCY_LONG, sizeof(char));
	uint8_t* rs_decoded = (uint8_t*) calloc(MSGSIZ_LONG,                   sizeof(char));
	uint8_t* packet     = (uint8_t*) calloc(len,                           sizeof(char));

	err = 0;
	net_len = len;

	if (len < REDUNDANCY_SHORT || len > (MSGSIZ_LONG + REDUNDANCY_LONG)) {
		net_len = 0;
		err = 999;

	} else if (len <= (MSGSIZ_SHORT + REDUNDANCY_SHORT)) {
		net_len -= REDUNDANCY_SHORT;
		memcpy(rs_encoded, packet_with_fec, net_len);
		memcpy(rs_encoded + MSGSIZ_SHORT, packet_with_fec + net_len, REDUNDANCY_SHORT);
		if (rsf_short.Decode(rs_encoded, rs_decoded)) {
			err = 998;
		}

	} else if (len <= (MSGSIZ_MEDIUM + REDUNDANCY_MEDIUM)) {
		net_len -= REDUNDANCY_MEDIUM;
		memcpy(rs_encoded, packet_with_fec, net_len);
		memcpy(rs_encoded + MSGSIZ_MEDIUM, packet_with_fec + net_len, REDUNDANCY_MEDIUM);
		if (rsf_medium.Decode(rs_encoded, rs_decoded)) {
			err = 997;
		}

	} else {
		net_len -= REDUNDANCY_LONG;
		memcpy(rs_encoded, packet_with_fec, net_len);
		memcpy(rs_encoded + MSGSIZ_LONG, packet_with_fec + net_len, REDUNDANCY_LONG);
		if (rsf_long.Decode(rs_encoded, rs_decoded)) {
			err = 996;
		}
	}
	
	free(rs_encoded);
	memcpy(packet, rs_decoded, net_len);
	free(rs_decoded);
	
	return packet;
}

uint8_t* LoRaL2::hashed_key(const char *key, size_t len)
{
	if (! key) {
		return 0;
	}

	Sha256 hash;
	hash.init();
	hash.write(2);
	for (size_t i = 0; i < len; ++i) {
		hash.write((uint8_t) key[i]);
	}
	const uint8_t* res = hash.result();

	AES256 aes256;
	uint8_t* hkey = (uint8_t*) calloc(aes256.keySize(), sizeof(uint8_t));
	for (size_t i = 0; i < aes256.keySize(); ++i) {
		hkey[i] = res[i % HASH_LENGTH]; // constant from sha256.h
	}

	return hkey;
}

// TODO cryptografically secure IV
void LoRaL2::gen_iv(uint8_t* buffer, size_t len)
{
	buffer[0] = CRYPTO_MAGIC;
	for (size_t i = 1; i < len; ++i) {
		buffer[i] = arduino_random(0, 256);
	}
}

uint8_t *LoRaL2::encrypt(const uint8_t *packet, size_t payload_len, size_t& tot_len)
{
	if (! hkey) {
		tot_len = payload_len;
		uint8_t* copy = (uint8_t*) malloc(payload_len);
		memcpy(copy, packet, payload_len);
		return copy;
	}

	AES256 aes256;
	uint8_t* ukey = (uint8_t*) calloc(sizeof(uint8_t), aes256.keySize());
	memcpy(ukey, hkey, aes256.keySize());
	aes256.setKey(ukey, aes256.keySize());
	free(ukey);

	tot_len = aes256.blockSize() + CRYPTO_LENGTH_LEN + payload_len;
	size_t enc_blocks = (tot_len - 1) / aes256.blockSize() + 1;
	tot_len = enc_blocks * aes256.blockSize();

	uint8_t* buffer = (uint8_t*) calloc(1, tot_len);
	gen_iv(buffer, aes256.blockSize());
	buffer[aes256.blockSize() + 0] = payload_len % 256;
	buffer[aes256.blockSize() + 1] = payload_len / 256;
	memcpy(buffer + aes256.blockSize() + CRYPTO_LENGTH_LEN, packet, payload_len);

	for (size_t i = 1; i < enc_blocks; ++i) {
		size_t offset_ant = (i - 1) * aes256.blockSize();
		size_t offset = i * aes256.blockSize();
		// pre-scramble using IV or previous block
		for (size_t j = 0; j < aes256.blockSize(); ++j) {
			buffer[offset + j] ^= buffer[offset_ant + j];
		}
		// encrypt
		aes256.encryptBlock(buffer + offset, buffer + offset);
	}

	return buffer;
}

uint8_t *LoRaL2::decrypt(const uint8_t *enc_packet, size_t tot_len, size_t& pay_len, int& err)
{
	uint8_t* packet = (uint8_t*) calloc(tot_len + 1, sizeof(uint8_t));

	if (!hkey) {
		err = 0;
		pay_len = tot_len;
		memcpy(packet, enc_packet, tot_len);
		return packet;
	}

	// if receiver has the wrong key, the payload will be mangled and will
	// be most probably rejected

	AES256 aes256;
	uint8_t* ukey = (uint8_t*) calloc(sizeof(uint8_t), aes256.keySize());
	memcpy(ukey, hkey, aes256.keySize());
	aes256.setKey(ukey, aes256.keySize());
	free(ukey);

	if (tot_len < (2 * aes256.blockSize())) {
		// packet too short
		err = 1001;
		pay_len = tot_len;
		memcpy(packet, enc_packet, tot_len);
		return packet;
	}

	if (tot_len % aes256.blockSize() != 0) {
		// packet not a multiple of block
		err = 1002;
		pay_len = tot_len;
		memcpy(packet, enc_packet, tot_len);
		return packet;
	}

	size_t blocks = tot_len / aes256.blockSize();

	uint8_t* buffer_interm = (uint8_t*) malloc(tot_len);
	memcpy(buffer_interm, enc_packet, tot_len);

	for (size_t i = blocks - 1; i >= 1; --i) {
		size_t offset = i * aes256.blockSize();
		size_t offset_ant = (i - 1) * aes256.blockSize();
		// decrypt
		aes256.decryptBlock(buffer_interm + offset, buffer_interm + offset);
		// de-scramble based on previous block
		for (size_t j = 0; j < aes256.blockSize(); ++j) {
			buffer_interm[offset + j] ^= buffer_interm[offset_ant + j];
		}
	}

	pay_len = buffer_interm[aes256.blockSize() + 0] +
			buffer_interm[aes256.blockSize() + 1] * 256;
	size_t calc_enc_len = aes256.blockSize() + CRYPTO_LENGTH_LEN + pay_len;
	size_t calc_blocks = (calc_enc_len - 1) / aes256.blockSize() + 1;

	if (blocks != calc_blocks) {
		// block incompatible with alleged payload length
		free(buffer_interm);
		err = 1003;
		pay_len = tot_len;
		memcpy(packet, enc_packet, tot_len);
		return packet;
	}

	memcpy(packet, buffer_interm + aes256.blockSize() + CRYPTO_LENGTH_LEN, pay_len);
	free(buffer_interm);

	err = 0;
	return packet;
}
