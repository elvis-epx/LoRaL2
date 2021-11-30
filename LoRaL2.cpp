#include <stdlib.h>
#include "RS-FEC.h"

#define POWER   20
#define PABOOST 1
#define CR4SLSH 5

#ifndef __AVR__
#include <SPI.h>
#endif
#include <LoRa.h>

#ifndef __AVR__

// Pin defintion of WIFI LoRa 32
// HelTec AutoMation 2017 support@heltec.cn 
#define SCK     5    // GPIO5  -- SX127x's SCK
#define MISO    19   // GPIO19 -- SX127x's MISO
#define MOSI    27   // GPIO27 -- SX127x's MOSI

#define SS      18   // GPIO18 -- SX127x's CS
#define RST     14   // GPIO14 -- SX127x's RESET
#define DIO0    26   // GPIO26 -- SX127x's IRQ(Interrupt Request)

#else

#define SS 8
#define RST 4
#define DIO0 7

#endif

#define STATUS_IDLE 0
#define STATUS_RECEIVING 1
#define STATUS_TRANSMITTING 2

#include "LoRaL2.h"

static LoRaL2* observer;
static void on_recv_trampoline(int len);
static void on_sent_trampoline();

LoRaL2::LoRaL2(long int band, int spread, int bandwidth,
		const char *key, int key_len, recv_callback recv_cb)
{
	observer = this;

	this->band = band;
	this->spread = spread;
	this->bandwidth = bandwidth;
	if (key) {
		this->key = (char*) malloc(key_len);
		::memcpy(this->key, key, key_len);
	} else {
		this->key = 0;
		this->key_len = 0;
	}
	this->recv_cb = recv_cb;

	this->status = STATUS_IDLE;

#ifndef __AVR__
	SPI.begin(SCK, MISO, MOSI, SS);
#endif
	LoRa.setPins(SS, RST, DIO0);

	if (!LoRa.begin(band)) {
		_ok = false;
	}

	LoRa.setTxPower(POWER, PABOOST);
	LoRa.setSpreadingFactor(spread);
	LoRa.setSignalBandwidth(bandwidth);
	LoRa.setCodingRate4(CR4SLSH);
	LoRa.disableCrc();
	LoRa.onReceive(on_recv_trampoline);
	LoRa.onTxDone(on_sent_trampoline);
	resume_rx();

	_ok = true;
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
		LoRa.receive();
	}
}

void LoRaL2::on_sent()
{
	status = STATUS_IDLE;
	resume_rx();
}

void LoRaL2::on_recv(int len)
{
	uint8_t *buffer = (uint8_t*) calloc(len, sizeof(char));

	int rssi = LoRa.packetRssi();
	for (size_t i = 0; i < len; i++) {
		buffer[i] = LoRa.read();
	}

	int packet_len = 0;
	int err = 0;
	uint8_t *packet = decode_fec(buffer, len, packet_len, err);
	free(buffer);

	recv_cb(new LoRaL2Packet(packet, packet_len, rssi, err));
}

bool LoRaL2::send(const uint8_t *packet, int len)
{
	if (status == STATUS_TRANSMITTING) {
		return false;
	}

	if (! LoRa.beginPacket()) {
		// can only fail if in tx mode, don't touch anything
		return false;
	}

	int tot_len;
	uint8_t* fec_packet = append_fec(packet, len, tot_len);

	status = STATUS_TRANSMITTING;
	LoRa.write(fec_packet, tot_len);
	LoRa.endPacket(true);

	free(fec_packet);

	return true;
}

// ugly
static void on_recv_trampoline(int len)
{
	observer->on_recv(len);
}

// ugly
static void on_sent_trampoline()
{
	observer->on_sent();
}

LoRaL2Packet::LoRaL2Packet(uint8_t *packet, int len, int rssi, int err)
{
	this->packet = (err ? 0 : packet);
	this->len = len;
	this->rssi = rssi;
	this->err = err;
}
	
LoRaL2Packet::~LoRaL2Packet()
{
	free(packet);
}

// FEC-related code

static const int MSGSIZ_SHORT = 50;
static const int MSGSIZ_MEDIUM = 100;
static const int MSGSIZ_LONG = 200;
static const int REDUNDANCY_SHORT = 10;
static const int REDUNDANCY_MEDIUM = 14;
static const int REDUNDANCY_LONG = 20;

RS::ReedSolomon<MSGSIZ_SHORT, REDUNDANCY_SHORT> rsf_short;
RS::ReedSolomon<MSGSIZ_MEDIUM, REDUNDANCY_MEDIUM> rsf_medium;
RS::ReedSolomon<MSGSIZ_LONG, REDUNDANCY_LONG> rsf_long;

int LoRaL2::max_payload() const
{
	return MSGSIZ_LONG;
}

uint8_t *LoRaL2::append_fec(const uint8_t* packet, int len, int& new_len)
{
	if (len > MSGSIZ_LONG) {
		len = MSGSIZ_LONG;
	} else if (len < 0) {
		len = 0;
	}
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

uint8_t *LoRaL2::decode_fec(const uint8_t* packet_with_fec, int len, int& net_len, int& err)
{
	uint8_t* rs_encoded = (uint8_t*) calloc(MSGSIZ_LONG + REDUNDANCY_LONG, sizeof(char));
	uint8_t* rs_decoded = (uint8_t*) calloc(MSGSIZ_LONG,                   sizeof(char));
	uint8_t* packet     = (uint8_t*) calloc(len,                           sizeof(char));

	err = 0;
	net_len = len;

	if (len < REDUNDANCY_SHORT || len > (MSGSIZ_LONG + REDUNDANCY_LONG)) {
		net_len = 0;
		err = 999;
	}

	if (len <= (MSGSIZ_SHORT + REDUNDANCY_SHORT)) {
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
