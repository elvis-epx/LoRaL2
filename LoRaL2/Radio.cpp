/*
 * LoRaL2 (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

#include "Radio.h"
#include "LoRaL2.h"

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

static LoRaL2* observer;

static void on_recv_trampoline(int len)
{
	uint8_t *buffer = (uint8_t*) calloc(len, sizeof(char));

	int rssi = LoRa.packetRssi();
	for (size_t i = 0; i < len; i++) {
		buffer[i] = LoRa.read();
	}

	observer->on_recv(rssi, buffer, len);
}

static void on_sent_trampoline()
{
	observer->on_sent();
}

bool lora_start(long int band, int spread, int bandwidth, int power, int paboost,
		int cr4slsh, LoRaL2 *pobserver)
{
#ifndef __AVR__
	SPI.begin(SCK, MISO, MOSI, SS);
#endif
	LoRa.setPins(SS, RST, DIO0);

	if (!LoRa.begin(band)) {
		return false;
	}

	LoRa.setTxPower(power, paboost);
	LoRa.setSpreadingFactor(spread);
	LoRa.setSignalBandwidth(bandwidth);
	LoRa.setCodingRate4(cr4slsh);
	LoRa.disableCrc();
	LoRa.onReceive(on_recv_trampoline);
	LoRa.onTxDone(on_sent_trampoline);
	
	observer = pobserver;
	return true;
}

void lora_receive()
{
	LoRa.receive();
}

bool lora_begin_packet()
{
	return LoRa.beginPacket();
}

void lora_finish_packet(const uint8_t* packet, size_t len)
{
	LoRa.write(packet, len);
	LoRa.endPacket(true);
}
