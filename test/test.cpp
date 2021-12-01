#include "LoRaL2.h"
#include <cstdlib>
#include <cstring>

#define BAND    916750000
#define SPREAD  7
#define BWIDTH  125000
#define ENCRYPTION_KEY "abracadabra"

void packet_received(LoRaL2Packet *pkt);

int main()
{
	LoRaL2* l2 = new LoRaL2(BAND, SPREAD, BWIDTH,
			ENCRYPTION_KEY, strlen(ENCRYPTION_KEY),
			packet_received);
}

void packet_received(LoRaL2Packet *pkt)
{
	delete pkt;
}
