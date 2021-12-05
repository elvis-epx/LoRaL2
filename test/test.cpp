#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "LoRaL2.h"
#include "ArduinoBridge.h"

extern uint8_t* lora_test_last_sent;
extern int lora_test_last_sent_len;
extern bool lora_emu_call_onsent;
extern bool lora_emu_sim_senderr;

static uint8_t* test_payload;
static size_t test_len;
static int test_exp_err_min;
static int test_exp_err_max;

static uint8_t* memdup(const uint8_t* buffer, size_t len)
{
	uint8_t* res = (uint8_t*) malloc(len);
	memcpy(res, buffer, len);
	return res;
}

class TestObserver: public LoRaL2Observer
{
public:
virtual void recv(LoRaL2Packet *pkt)
{
	printf("\tReceived len %lu err %d\n", pkt->len, pkt->err);
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
};

static TestObserver *observer = 0;

#define BAND 915000000
#define SPREAD 7
#define BWIDTH 125000

static void test_1(const char *key)
{
	LoRaL2* l2 = new LoRaL2(BAND, SPREAD, BWIDTH,
			key, key ? strlen(key) : 0,
			observer);
	printf("Status %d, Speed in bps: %d\n", !!l2->ok(), l2->speed_bps());

	uint8_t *recv_buffer;
	size_t recv_len;

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
		printf("\tReceiving SDR len %lu\n", recv_len);
		l2->on_recv(-50, recv_buffer, recv_len);

		// short packet FEC
		test_exp_err_min = test_exp_err_max = 999;
		recv_len = 9;
		recv_buffer = (uint8_t*) malloc(recv_len);
		printf("\tReceiving short len %lu\n", recv_len);
		l2->on_recv(-50, recv_buffer, recv_len);

		// long packet FEC
		test_exp_err_min = test_exp_err_max = 999;
		recv_len = 300;
		recv_buffer = (uint8_t*) malloc(recv_len);
		printf("\tReceiving long len %lu\n", recv_len);
		l2->on_recv(-50, recv_buffer, recv_len);

		// cleanup
		free(lora_test_last_sent);
		lora_test_last_sent = 0;
		free(test_payload);
	}

	// hazing tests, should not go past FEC
	// FIXME making RS-FEC to abort, at least in MacOS, check why
	for (int i = 0; i < 10000; ++i) {
		test_exp_err_min = 1;
		test_exp_err_max = 2000;
		recv_len = random() % 300;
		recv_buffer = (uint8_t*) malloc(recv_len);
		for (size_t i = 0; i < recv_len; ++i) {
			recv_buffer[i] = random() % 256;
		}
		printf("\tHazing #%d len %lu\n", i, recv_len);
		l2->on_recv(-50, recv_buffer, recv_len);
	}

	delete l2;
}

static uint8_t hc_enc[] = {5, 183, 73, 105, 43, 39, 69, 61, 255, 30, 89, 201, 11, 134, 253, 62,
			106, 127, 124, 123, 173, 100, 90, 46, 197, 2, 254, 181, 0, 143, 22, 164};
static const size_t hc_enc_len = 32;
static const char *hc_unenc = "highcastle";

static void test_encryption()
{
	LoRaL2* l2 = new LoRaL2(BAND, SPREAD, BWIDTH,
			"abracadabra", strlen("abracadabra"),
			0);

	size_t len;
	int err;
	uint8_t* res;

	res = l2->decrypt(hc_enc, hc_enc_len, len, err);
	if (err != 0) {
		printf("Decryption test: unexpected err %d\n", err);
		exit(1);
	}
	if (len != strlen(hc_unenc)) {
		printf("Decryption test: bad len %lu\n", len);
		exit(1);
	}
	for (size_t i = 0 ; i < len; ++i) {
		if (res[i] != hc_unenc[i]) {
			printf("\t\tDecryption test: mismatched octet %lu\n", i);
			exit(1);
		}
	}
	free(res);

	// 1 too short
	res = l2->decrypt(hc_enc, hc_enc_len - 1, len, err);
	if (err != 1001) {
		printf("Decryption test: unexpected err %d\n", err);
		exit(1);
	}
	free(res);

	// 1 too long
	res = l2->decrypt(hc_enc, hc_enc_len + 1, len, err);
	if (err != 1002) {
		printf("Decryption test: unexpected err %d\n", err);
		exit(1);
	}
	free(res);

	// corrupted internal length
	hc_enc[16] = 99;
	res = l2->decrypt(hc_enc, hc_enc_len, len, err);
	if (err != 1003) {
		printf("Decryption test: unexpected err %d\n", err);
		exit(1);
	}
	free(res);

	delete l2;
}

int main()
{
	// calls srandom(time of day) indirectly
	arduino_random(0, 2);

	lora_emu_call_onsent = false;
	lora_emu_sim_senderr = false;

	observer = new TestObserver();
	test_encryption();
	test_1(0);
	test_1("abracadabra");
	test_1("");
	delete observer;
}
