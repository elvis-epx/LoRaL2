#include <stdlib.h>

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

#include "LoraL2.h"

static LoraL2* observer;
static void on_recv_trampoline(int len);
static void on_sent_trampoline();

LoraL2::LoraL2(long int band, int spread, int bandwidth,
		const char *key, int key_len, recv_callback recv_cb)
{
	observer = this;

	this->band = band;
	this->spread = spread;
	this->bandwidth = bandwidth;
	if (key) {
		this->key = (char*) ::malloc(key_len);
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

bool LoraL2::ok() const
{
	return _ok;
}

uint32_t LoraL2::speed_bps() const
{
	uint32_t bps = bandwidth;
	bps *= spread;
	bps *= 4;
	bps /= CR4SLSH;
	bps /= (1 << spread);
	return bps;
}

void LoraL2::resume_rx()
{
	if (status != STATUS_RECEIVING) {
		status = STATUS_RECEIVING;
		LoRa.receive();
	}
}

void LoraL2::on_sent()
{
	status = STATUS_IDLE;
	resume_rx();
}

void LoraL2::on_recv(int len)
{
	uint8_t *buffer = (uint8_t*) calloc(len + 1, sizeof(char));

	int rssi = LoRa.packetRssi();
	for (size_t i = 0; i < len; i++) {
		buffer[i] = LoRa.read();
	}

	recv_cb(new LoRaL2Packet(buffer, len, rssi, 0));
}

bool LoraL2::send(const uint8_t *packet, int len)
{
	if (status == STATUS_TRANSMITTING) {
		return false;
	}

	if (! LoRa.beginPacket()) {
		// can only fail if in tx mode, don't touch anything
		return false;
	}

	status = STATUS_TRANSMITTING;
	LoRa.write(packet, len);
	LoRa.endPacket(true);
	return true;
}

static void on_recv_trampoline(int len)
{;
	observer->on_recv(len);
}

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
	
LoRaL2Packet::~LoRaL2Packet() {
	free(packet);
}
