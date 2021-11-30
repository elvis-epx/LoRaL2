#include "SSD1306.h"
#include "LoRaL2.h"

LoRaL2 *l2;

#define BAND    916750000
#define SPREAD  7
#define BWIDTH  125000

#define MY_NAME "AA "

void packet_received(LoRaL2Packet *pkt);

void setup()
{
	Serial.begin(115200);
	Serial.println("Starting");
	const char *encryption_key = 0;
	l2 = new LoRaL2(BAND, SPREAD, BWIDTH, 0, 0, packet_received);
	if (l2->ok()) {
		Serial.println("Started");
	} else {
    // may happen in case some LoRa parameter is wrong
		Serial.println("NOT started");
	}
	oled_init();
}

static LoRaL2Packet *pending_recv = 0;
long int next_send = millis() + 5000 + random(1, 15000);

void loop()
{	
	handle_received_packet();
	send_packet();
}

void send_packet() {
	if (millis() < next_send) {
		return;
	}

	String msg = MY_NAME + String(millis());
	Serial.print("Sending: ");
	Serial.println(msg);
	if (!l2->send((const uint8_t*) msg.c_str(), msg.length())) {
		Serial.println("Send error");
		oled_show("Send error");
	} else {
		String msg2 = "Sent " + String(millis());
		oled_show(msg2.c_str());
	}
	
	next_send = millis() + 5000 + random(1, 15000);
}

void handle_received_packet()
{
	if (! pending_recv) {
		return;
	}

	if (pending_recv->err) {
		Serial.print("Recv with err ");
		Serial.println(pending_recv->err);
	} else {
		Serial.print("Recv ");
		Serial.print((const char *) pending_recv->packet);
		Serial.print("  Len ");
		Serial.print(pending_recv->len);
		String msg = "Recv ";
		msg += (const char*) pending_recv->packet;
		oled_show(msg.c_str());
	}
	Serial.print(" RSSI ");
	Serial.println(pending_recv->rssi);

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

void oled_show(const char* msg)
{
        display.clear();
        display.drawString(0, 0, msg);
        display.display();
}
