CC       = g++
CFLAGS   = -std=c++11 -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -D__BASEFILE__=\"$*\"
LIBS     = -lwiringPi -lbcm2835 -pthread -lboost_system -lopencv_core -lopencv_photo
LMICBASE = libs/lmic/src
INCLUDE  = -I$(LMICBASE) -Ilibs/
BUILD_DIR  = build/

all: main command_prompt

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

$(BUILD_DIR)command_prompt.o: command_prompt.cpp
				$(CC) -o $(BUILD_DIR)/command_prompt.o $(CFLAGS) -c $(INCLUDE) $<

main: $(BUILD_DIR)main.o $(BUILD_DIR)raspi.o $(BUILD_DIR)radio.o $(BUILD_DIR)oslmic.o $(BUILD_DIR)lmic.o $(BUILD_DIR)hal.o $(BUILD_DIR)aes.o
				$(CC) $^ $(LIBS) -o main

command_prompt: $(BUILD_DIR)command_prompt.o $(BUILD_DIR)raspi.o $(BUILD_DIR)radio.o $(BUILD_DIR)oslmic.o $(BUILD_DIR)lmic.o $(BUILD_DIR)hal.o $(BUILD_DIR)aes.o
				$(CC) $^ $(LIBS) -o command_prompt

clean:
				rm -rf build/*.o main command_prompt
				rm -rf *.o main command_prompt
