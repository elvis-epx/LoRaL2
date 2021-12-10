# LoRa-trans - L2 transport improvements for LoRa

This project adds a layer over LoRa transport, as well as an
easy-to-use API. The idea is to make LoRa easier to use in IoT projects.

Check the Arduino sketch which is a complete example of API usage.

Our target hardware is TTGO-ESP32, so the low-level code and testing is
tested only on this platform.

LoRa-trans adds FEC (Forward Error Correction) and optional encryption
to LoRa packets.

## Error correction

LoRa has layer-1 error correction (named CR) as well as layer-1 error
detection using CRC. Why bother with software FEC?

In our experiments, we found that many LoRa packets arrive with small errors, even in 
the best circunstances. CR is not enough to prevent this, even when maxed out (8/4).
Let alone that CR is a very wasteful error-correction coding, eating up 50% of the
bandwidth in 8/4 mode.

Due to this, when layer-1 CRC is activated, many almost-perfect packets are discarded.
Note that LoRa CRC is 16 bit only, so it is not a strong guarantee against corrupted
data.

LoRa's CR and CRC codes were probably chosen to be fast in hardware and spend little
energy, but even the good old Reed-Solomon can do much better. A good FEC code increases the
usable radio range and guarantees the application layer won't get corrupted data.

Every packet is trailed by a FEC (Forward Error Code). The code can be
Reed-Solomon RS(50,10), RS(100,14) or RS(230,20) depending on packet
size. This means the maximum payload is 230 octets.

Since Reed-Solomon codes demand a fixed-size message, it is calculated as if
the network packet was padded with nulls (binary zeros) up to 50, 100 or 230
octets.

The idea of using several codes is to keep the redundancy and error correction
power roughly similar to all packet sizes, with a discreet advantage given to
shorter packets.

The reference FEC RS implementation is https://github.com/simonyipeter/Arduino-FEC .

Packets are transmitted using LoRa explicit mode, so the payload size can be inferred.
CRC is disabled. CR is set to 5/4, the weakest allowed by LoRa.

## Encryption

Optional encryption is based on AES256 cypher.

If encryption is on, maximum payload is 198 octets, due to IV preamble and block round-up.

## Testing

Unit testing is carried out on PC, given the superior tools (code coverage,
Valgrind) available. Embedded APIs are mocked up.

This project uses slightly modified versions of RS-FEC, AES256, and SHA-256 
libraries. Their copyrights belong to their respective authors (mentioned 
directly or indirectly in the header of each file).

# LoRa parameters

In LoRaL2 constructor, some LoRa parameters (frequency, spread and bandwidth)
are pased as parameters. The suggested parameters in Lora-trans.ino deliver
5kbps of speed and have a good balance between reliability, range and speed,
especially for testing.

If you need really good range, try to reduce bandwidth to 62500 (if your LoRa
chip supports it) and increase spread to SF9. This is 9dB better than the
project default.  We have had very good range in urban areas with this setup,
and effortlessly reach several km of range in almost-clear sight. 
The downside of longer range is low speed (0.8kbps).

Another almost equivalent recipe is 125kHz bandwidth and SF10. In our tests,
the narrower bandwidth did better, but your mileage may vary depending on
your hardware and other conditions.

Be sure to generate very few packets, with very small payloads, if you go
low speed.  An encrypted packet has a minimum of 44 octets, which takes half
a second in 0.8kbps speed!

The project disables CRC mode, enables explicit mode and uses the lowest
L1 redundancy possible (5/4). This is hardcoded within Radio.cpp, and should
be kept as they are. In particular the explicit mode should not be turned off,
since we depend on it.

# License

The code authored by us is distributed under MIT license.
