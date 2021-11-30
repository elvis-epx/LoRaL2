#include "CryptoKeys.h"

static Ptr<Packet> decode_l2b(const char* data, size_t len, int rssi, int &error,
				bool maybe_encrypted)
{
	if (!maybe_encrypted) {
		return Packet::decode_l3(data, len, rssi, error, false);
	}

	char *udata;
	size_t ulen;
	Ptr<Packet> p;
	int decrypt_res = CryptoKeys::decrypt(data, len, &udata, &ulen);

	if (decrypt_res == CryptoKeys::OK_DECRYPTED) {
		p = Packet::decode_l3(udata, ulen, rssi, error, true);
		::free(udata);
	} else if (decrypt_res == CryptoKeys::OK_CLEARTEXT) {
		p = Packet::decode_l3(data, len, rssi, error, false);
	} else if (decrypt_res == CryptoKeys::ERR_NOT_ENCRYPTED) {
		error = 1900;
	} else if (decrypt_res == CryptoKeys::ERR_ENCRYPTED) {
		error = 1901;
	} else if (decrypt_res == CryptoKeys::ERR_DECRIPTION) {
		error = 1902;
	}

	return p;
}

