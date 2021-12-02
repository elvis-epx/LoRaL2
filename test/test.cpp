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
int test_exp_err_min;
int test_exp_err_max;

static uint8_t* memdup(const uint8_t* buffer, size_t len)
{
	uint8_t* res = (uint8_t*) malloc(len);
	memcpy(res, buffer, len);
	return res;
}

void test_1_packet_received(LoRaL2Packet *pkt)
{
	printf("\tReceived len %lu\n", pkt->len);
	if (pkt->err < test_exp_err_min || pkt->err > test_exp_err_max) {
		printf("\t\tUnexpected error %d, expected %d..%d\n",
			pkt->err, test_exp_err_min, test_exp_err_max);
		exit(1);
	}
	if (test_exp_err_min == 0 && test_exp_err_max == 0) {
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

	for (size_t len = 0; len <= l2->max_payload() + 1; ++len) {
		test_payload = (uint8_t*) malloc(len);
		test_len = len;
		for (size_t i = 0; i < len; ++i) {
			test_payload[i] = random() % 256;
		}

		printf("Sending len %lu\n", len);
		if (len <= l2->max_payload()) {
			if (!l2->send(test_payload, len)) {
				printf("send() failed\n");
				exit(1);
			}
			if (l2->send((const uint8_t*) "", 0)) {
				printf("re-send() before on_sent() did not fail\n");
				exit(1);
			}
		} else {
			if (l2->send(test_payload, len)) {
				printf("send() should have failed (payload too big)\n");
				exit(1);
			}
			free(test_payload);
			continue;
		}

		l2->on_sent();

		// TODO test if it was really encrypted

		uint8_t *recv_buffer;
		size_t recv_len;

		// reception of perfect packet
		test_exp_err_min = test_exp_err_max = 0;
		recv_len = lora_test_last_sent_len;
		recv_buffer = memdup(lora_test_last_sent, recv_len);
		printf("\tReceiving len %lu\n", recv_len);
		l2->on_recv(-50, recv_buffer, recv_len);

		// light data corrruption
		test_exp_err_min = test_exp_err_max = 0;
		recv_len = lora_test_last_sent_len;
		recv_buffer = memdup(lora_test_last_sent, recv_len);
		for (size_t i = 0; i < 5; ++i) {
			recv_buffer[random() % recv_len] = random() % 256;
		}
		printf("\tReceiving LDR len %lu\n", recv_len);
		l2->on_recv(-50, recv_buffer, recv_len);

		// severe data corrruption
		if (key) {
			// final L1 size boring to estimate because of encryption block
			test_exp_err_min = 996;
			test_exp_err_max = 998;
		} else {
			if (test_len <= 50) {
				test_exp_err_min = test_exp_err_max = 998;
			} else if (test_len <= 100) {
				test_exp_err_min = test_exp_err_max = 997;
			} else {
				test_exp_err_min = test_exp_err_max = 996;
			}
		}
		recv_len = lora_test_last_sent_len;
		recv_buffer = memdup(lora_test_last_sent, recv_len);
		for (size_t i = 0; i < 30; ++i) {
			recv_buffer[random() % recv_len] = random() % 256;
		}
		printf("\tReceiving LDR len %lu\n", recv_len);
		l2->on_recv(-50, recv_buffer, recv_len);

		// cleanup
		free(lora_test_last_sent);
		lora_test_last_sent = 0;
		free(test_payload);
	}

	delete l2;
}

int main()
{
	// calls srandom(time of day) indirectly
	arduino_random(0, 2);

	test_1(0);
	test_1("abracadabra");
	test_1("");
}
