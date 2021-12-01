/*
 * LoRa-trans (LoRa layer-2) project
 * Copyright (c) 2021 PU5EPX
 */

#include "SSD1306.h"
#include "LoRaL2.h"
#include "ArduinoBridge.h"
#include "LoRaParams.h"

LoRaL2 *l2;
char myid[5];

#define ENCRYPTION_KEY "abracadabra"

#define HALF_SEND_INTERVAL 5000

void packet_received(LoRaL2Packet *pkt);
void oled_show(const char* msg, const char *msg2 = 0);

void setup()
{
	uint64_t chipid = ESP.getEfuseMac();
	uint16_t hash = (uint16_t)(chipid >> 32);
	snprintf(myid, 5, "%04X", hash);

	Serial.begin(115200);
	Serial.print("Starting, ID is ");
	Serial.println(myid);
	l2 = new LoRaL2(BAND, SPREAD, BWIDTH,
			ENCRYPTION_KEY, strlen(ENCRYPTION_KEY),
			packet_received);
	if (l2->ok()) {
		Serial.println("Started");
	} else {
		// may happen in case some LoRa parameter is wrong
		Serial.println("NOT started");
	}
	oled_init();
}

static LoRaL2Packet *pending_recv = 0;
long int next_send = arduino_millis() + HALF_SEND_INTERVAL +
			arduino_random(0, HALF_SEND_INTERVAL * 2);

void loop()
{	
	handle_received_packet();
	send_packet();
}

void send_packet()
{
	if (arduino_millis() < next_send) {
		return;
	}

	String payload = String(myid) + " " + String(arduino_millis() / 1000) + "s ";
	int send_size = arduino_random(6, l2->max_payload() + 1);
	while (payload.length() < send_size) {
		payload += ".";
	}

	String msg = "Send ";
	String msg2 = "  len " + String(payload.length());
	
	if (!l2->send((const uint8_t*) payload.c_str(), payload.length())) {
		msg += " (ERROR)";
	} else {
		msg += payload;
	}

	Serial.println(msg);
	Serial.println(msg2);
	oled_show(msg.c_str(), msg2.c_str());
	
	next_send = arduino_millis() + HALF_SEND_INTERVAL +
			arduino_random(0, HALF_SEND_INTERVAL * 2);
}

void handle_received_packet()
{
	if (! pending_recv) {
		return;
	}

	String msg;
	String msg2 = "  RSSI " + String(pending_recv->rssi) + " len " + String(pending_recv->len);
	if (pending_recv->err) {
		msg = "Recv with ERROR " + String(pending_recv->err);
	} else {
		msg = "Recv " + String((const char *) pending_recv->packet);
	}
	
	Serial.println(msg);
	Serial.println(msg2);
	oled_show(msg.c_str(), msg2.c_str());

	delete pending_recv;
	pending_recv = 0;
}


// RX callback
// Takes the ownership of pkt and its internal buffer
// Do as little as possible here (e.g. oled_show() may crash)

void packet_received(LoRaL2Packet *pkt)
{
	if (pending_recv) {
		delete pending_recv;
	}
	pending_recv = pkt;
}

SSD1306 display(0x3c, 4, 15);

void oled_init()
{
	pinMode(16, OUTPUT);
	pinMode(25, OUTPUT);

	digitalWrite(16, LOW); // reset
	delay(50);
	digitalWrite(16, HIGH); // keep high while operating display

	display.init();
	display.flipScreenVertically();
	display.setFont(ArialMT_Plain_10);
	display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void oled_show(const char* msg, const char *msg2)
{
	display.clear();
	display.drawString(0, 0, msg);
	if (msg2) {
		display.drawString(0, 24, msg2);
	}
	display.display();
}
