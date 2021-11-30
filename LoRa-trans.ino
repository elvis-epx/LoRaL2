#include "SSD1306.h"
#include "LoRaL2.h"

LoRaL2 *l2;
char myid[5];

#define BAND    916750000
#define SPREAD  7
#define BWIDTH  125000

#define HALF_SEND_INTERVAL 5000

void packet_received(LoRaL2Packet *pkt);

void setup()
{
	uint64_t chipid = ESP.getEfuseMac();
	uint16_t hash = (uint16_t)(chipid >> 32);
	snprintf(myid, 5, "%04X", hash);

	Serial.begin(115200);
	Serial.print("Starting, ID is ");
	Serial.println(myid);
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
long int next_send = millis() + HALF_SEND_INTERVAL + random(1, HALF_SEND_INTERVAL);

void loop()
{	
	handle_received_packet();
	send_packet();
}

void send_packet()
{
	if (millis() < next_send) {
		return;
	}

	String msg = String(myid) + " " + String(millis() / 1000) + "s ";
	int send_size = random(6, l2->max_payload());
	while (msg.length() < send_size) {
		msg += ".";
	}
	
	Serial.print("Sending len ");
	Serial.print(msg.length());
	Serial.print(" ");
	Serial.println(msg);
	if (!l2->send((const uint8_t*) msg.c_str(), msg.length())) {
		Serial.println("Send error");
		oled_show("Send error");
	} else {
		String msg2 = "Sent " + msg;
		oled_show(msg2.c_str());
	}
	
	next_send = millis() + HALF_SEND_INTERVAL + random(1, HALF_SEND_INTERVAL);

}

void handle_received_packet()
{
	if (! pending_recv) {
		return;
	}

	Serial.print("Recv RSSI ");
	Serial.print(pending_recv->rssi);
	Serial.print(" len ");
	Serial.print(pending_recv->len);
	
	if (pending_recv->err) {
		Serial.print(" with err ");
		Serial.println(pending_recv->err);
	} else {
		Serial.print(" pay ");
		Serial.println((const char *) pending_recv->packet);
		String msg = "Recv ";
		msg += (const char*) pending_recv->packet;
		oled_show(msg.c_str());
	}

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
