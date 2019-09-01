CC       = g++
CFLAGS   = -std=c++11 -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -D__BASEFILE__=\"$*\"
LIBS     = -lbcm2835
LMICBASE = libs/raspi-lmic/src
INCLUDE  = -I$(LMICBASE)
BUILD_DIR  = build/

all: main

$(BUILD_DIR)raspi.o: $(LMICBASE)/raspi/raspi.cpp
				$(CC) -o $(BUILD_DIR)/raspi.o $(CFLAGS) -c $(LMICBASE)/raspi/raspi.cpp $(INCLUDE)

$(BUILD_DIR)radio.o: $(LMICBASE)/lmic/radio.c
				$(CC) -o $(BUILD_DIR)/radio.o $(CFLAGS) -c $(LMICBASE)/lmic/radio.c $(INCLUDE)

$(BUILD_DIR)oslmic.o: $(LMICBASE)/lmic/oslmic.c
				$(CC) -o $(BUILD_DIR)/oslmic.o $(CFLAGS) -c $(LMICBASE)/lmic/oslmic.c $(INCLUDE)

$(BUILD_DIR)lmic.o: $(LMICBASE)/lmic/lmic.c
				$(CC) -o $(BUILD_DIR)/lmic.o $(CFLAGS) -c $(LMICBASE)/lmic/lmic.c $(INCLUDE)

$(BUILD_DIR)hal.o: $(LMICBASE)/hal/hal.cpp
				$(CC) -o $(BUILD_DIR)/hal.o $(CFLAGS) -c $(LMICBASE)/hal/hal.cpp $(INCLUDE)

$(BUILD_DIR)aes.o: $(LMICBASE)/aes/lmic.c
				$(CC) -o $(BUILD_DIR)/aes.o $(CFLAGS) -c $(LMICBASE)/aes/lmic.c $(INCLUDE)

$(BUILD_DIR)main.o: main.cpp
				$(CC) -o $(BUILD_DIR)/main.o $(CFLAGS) -c $(INCLUDE) $<

main: $(BUILD_DIR)main.o $(BUILD_DIR)raspi.o $(BUILD_DIR)radio.o $(BUILD_DIR)oslmic.o $(BUILD_DIR)lmic.o $(BUILD_DIR)hal.o $(BUILD_DIR)aes.o
				$(CC) $^ $(LIBS) -o main

clean:
				rm -rf build/*.o main
				rm -rf *.o main
