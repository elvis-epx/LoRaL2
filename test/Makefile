CFLAGS=-DDEBUG -DUNDER_TEST -fsanitize=undefined -fstack-protector-strong -fstack-protector-all -std=c++1y -Wall -g -O0 -fprofile-arcs -ftest-coverage -fno-elide-constructors
OBJ=FakeArduino.o BlockCipher.o AES256.o AESCommon.o Crypto.o LoRaL2.o sha256.o

all: test

clean:
	rm -rf *.o test *.gcda *.gcno *.info out *.dSYM *.log *.val *.gcov

.cpp.o: *.h
	gcc $(CFLAGS) -c $<

test: test.cpp $(OBJ) *.h
	gcc $(CFLAGS) -o test test.cpp $(OBJ) -lstdc++

recov:
	rm -f *.gcda

coverage:
	rm -f *.info
	gcov test.cpp
	gcov testnet.cpp
	gcov testnet2.cpp
	lcov -c --directory . --output-file main_coverage.info
	lcov -r main_coverage.info '*/RS-FEC.h' -o main_coverage.info
	lcov -r main_coverage.info '*/FakeArduino.cpp' -o main_coverage.info
	lcov -r main_coverage.info '*/Serial.cpp' -o main_coverage.info
	lcov -r main_coverage.info '*/sha256.h' -o main_coverage.info
	lcov -r main_coverage.info '*/test.cpp' -o main_coverage.info
	lcov -r main_coverage.info '*/sha256.cpp' -o main_coverage.info
	lcov -r main_coverage.info '*/Crypto.h' -o main_coverage.info
	lcov -r main_coverage.info '*/Crypto.cpp' -o main_coverage.info
	lcov -r main_coverage.info '*/AESCommon.cpp' -o main_coverage.info
	lcov -r main_coverage.info '*/AES256.cpp' -o main_coverage.info
	lcov -r main_coverage.info '*/BlockCipher.cpp' -o main_coverage.info
	genhtml main_coverage.info --output-directory out
	xdg-open out/index.html
