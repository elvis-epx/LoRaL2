#include "LoRaL2.h"
#include "LoRaParams.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

#define ENCRYPTION_KEY "abracadabra"

void packet_received(LoRaL2Packet *pkt);

extern uint8_t* lora_test_last_sent;
extern int lora_test_last_sent_len;

uint8_t* test_payload;
int test_len;

int main()
{
	LoRaL2* l2 = new LoRaL2(BAND, SPREAD, BWIDTH,
			ENCRYPTION_KEY, strlen(ENCRYPTION_KEY),
			packet_received);
	printf("Status %d, Speed in bps: %d\n", !!l2->ok(), l2->speed_bps());

	for (int len = 0; len <= l2->max_payload(); ++len) {
		test_payload = (uint8_t*) malloc(len);
		test_len = len;
		for (int i = 0; i < len; ++i) {
			test_payload[i] = random() % 256;
		}

		printf("Sending len %d\n", len);
		l2->send(test_payload, len);

		l2->on_sent();

		// TODO test if it was really encrypted

		printf("\tReceiving len %d\n", lora_test_last_sent_len);
		l2->on_recv(-50, lora_test_last_sent, lora_test_last_sent_len);
		// on_recv() takes ownership of pointer
		lora_test_last_sent = 0;

		free(test_payload);
	}

	delete l2;
}

void packet_received(LoRaL2Packet *pkt)
{
	printf("\tReceived len %d\n", pkt->len);
	if (pkt->len != test_len) {
		printf("\t\tMismatched length\n");
		exit(1);
	}
	for (int i = 0 ; i < test_len; ++i) {
		if (pkt->packet[i] != test_payload[i]) {
			printf("\t\tMismatched octet %d\n", i);
			exit(1);
		}
	}
	delete pkt;
}
