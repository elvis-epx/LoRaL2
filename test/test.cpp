#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "LoRaL2.h"
#include "LoRaParams.h"
#include "ArduinoBridge.h"

extern uint8_t* lora_test_last_sent;
extern int lora_test_last_sent_len;

uint8_t* test_payload;
size_t test_len;
int test_exp_err;

void test_1_packet_received(LoRaL2Packet *pkt)
{
	printf("\tReceived len %lu\n", pkt->len);
	if (pkt->err != test_exp_err) {
		printf("\t\tUnexpected error %d, expected %d\n", pkt->err, test_exp_err);
		exit(1);
	}
	if (!test_exp_err) {
		if (pkt->len != test_len) {
			printf("\t\tMismatched length\n");
			exit(1);
		}
		for (size_t i = 0 ; i < test_len; ++i) {
			if (pkt->packet[i] != test_payload[i]) {
				printf("\t\tMismatched octet %lu\n", i);
				exit(1);
			}
		}
	}
	delete pkt;
}

void test_1(const char *key)
{
	LoRaL2* l2 = new LoRaL2(BAND, SPREAD, BWIDTH,
			key, key ? strlen(key) : 0,
			test_1_packet_received);
	printf("Status %d, Speed in bps: %d\n", !!l2->ok(), l2->speed_bps());

	for (size_t len = 0; len <= l2->max_payload(); ++len) {
		test_payload = (uint8_t*) malloc(len);
		test_len = len;
		for (size_t i = 0; i < len; ++i) {
			test_payload[i] = random() % 256;
		}

		printf("Sending len %lu\n", len);
		if (!l2->send(test_payload, len)) {
			printf("send() failed\n");
			exit(1);
		}
		if (l2->send((const uint8_t*) "", 0)) {
			printf("re-send() before on_sent() did not fail\n");
			exit(1);
		}

		l2->on_sent();

		// TODO test if it was really encrypted

		// reception of perfect packet

		test_exp_err = 0;
		printf("\tReceiving len %d\n", lora_test_last_sent_len);
		l2->on_recv(-50, lora_test_last_sent, lora_test_last_sent_len);
		// on_recv() takes ownership of pointer
		lora_test_last_sent = 0;

		free(test_payload);
	}

	delete l2;
}

int main()
{
	// calls srandom(time of day) indirectly
	arduino_random(0, 2);

	test_1("abracadabra");
	test_1("");
	test_1(0);
}
