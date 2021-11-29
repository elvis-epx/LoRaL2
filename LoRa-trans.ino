#include "SSD1306.h"
#include "LoraL2.h"

LoraL2 *l2;

#define BAND    916750000
#define SPREAD  7
#define BWIDTH  125000

void setup()
{
	Serial.begin(115200);
	Serial.println("Starting");
	const char *encryption_key = 0;
	l2 = new LoraL2(BAND, SPREAD, BWIDTH, 0, 0, packet_received);
	if (l2->ok()) {
		Serial.println("Started");
	} else {
		Serial.println("NOT started");
	}
	oled_init();
}

void loop()
{
	delay(5000 + random(1, 5000));
	String msg = "Uptime " + String(millis());
	Serial.print("Sending: ");
	Serial.println(msg);
	if (!l2->send((const uint8_t*) msg.c_str(), msg.length())) {
		Serial.println("Sending");
	}
	String msg2 = "Sent " + String(millis());
	oled_show(msg2.c_str());
}

void packet_received(const uint8_t *packet, int len, int rssi, int err)
{
	if (err) {
		Serial.print("Recv with err ");
		Serial.println(err);
	} else {
		Serial.print("Recv: ");
		Serial.print((const char *) packet);
		Serial.print("  Len ");
		Serial.print(len);
		String msg = "Recv " + String(millis());
		oled_show(msg.c_str());
	}
	Serial.print(" RSSI ");
	Serial.println(rssi);
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

void oled_show(const char* msg) {}

/*
void oled_show(const char* msg)
{
        display.clear();
        display.drawString(0, 0, msg);
        display.display();
}
*/
