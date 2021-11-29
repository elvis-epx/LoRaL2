#ifndef __LORAL2_H
#define __LORAL2_H

typedef void (*recv_callback)(const uint8_t *packet, int len, int rssi, int err);

class LoraL2 {
public:
	LoraL2(long int band, int spread, int bandwidth,
		const char *key, int key_len, recv_callback recv_cb);
	bool send(const uint8_t *packet, int len);
	uint32_t speed_bps() const;
	bool ok() const;
	// public because LoRa C API needs to call them
	void on_recv(int len);
	void on_sent();

private:
	void resume_rx();

	long int band;
	int spread;
	int bandwidth;
	char *key;
	int key_len;
	recv_callback recv_cb;
	int status;
	bool _ok;
};

#endif
