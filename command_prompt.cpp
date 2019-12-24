#include <lmic.h>
#include <iostream>
#include <thread>
#include <cstring>
#include <hal/hal.h>
#include <opencv2/opencv.hpp>

#if !defined(DISABLE_INVERT_IQ_ON_RX)
#error This example requires DISABLE_INVERT_IQ_ON_RX to be set. Update \
       config.h in the lmic library to set it.
#endif

// How often to send a packet. Note that this sketch bypasses the normal
// LMIC duty cycle limiting, so when you change anything in this sketch
// (payload length, frequency, spreading factor), be sure to check if
// this interval should not also be increased.
// See this spreadsheet for an easy airtime and duty cycle calculator:
// https://docs.google.com/spreadsheets/d/1voGAtQAjC1qBmaVuP1ApNKs1ekgUjavHuVQIXyYSvNc
#define TX_INTERVAL 2000

// Dragino Raspberry PI hat (no onboard led)
// see https://github.com/dragino/Lora
#define RF_CS_PIN  RPI_V2_GPIO_P1_22 // Slave Select on GPIO25 so P1 connector pin #22
#define RF_IRQ_PIN RPI_V2_GPIO_P1_07 // IRQ on GPIO4 so P1 connector pin #7
#define RF_RST_PIN RPI_V2_GPIO_P1_11 // Reset on GPIO17 so P1 connector pin #11

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss  = RF_CS_PIN,
    .rxtx = LMIC_UNUSED_PIN,
    .rst  = RF_RST_PIN,
    .dio  = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
};

#ifndef RF_LED_PIN
#define RF_LED_PIN NOT_A_PIN
#endif

std::string tx_buffer = "basestation online";

// telemetry = tele, image transmission = rx_img
std::string rx_mode = "tele";

// 5000 equals a grey scale 71*71 image
unsigned char img_buffer[6500];

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

void onEvent (ev_t ev) {
}

osjob_t txjob;
osjob_t timeoutjob;
static void tx_func (osjob_t* job);

// Transmit the given string and call the given function afterwards
void tx(const char *str, osjobcb_t func) {
  os_radio(RADIO_RST); // Stop RX first
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet
  LMIC.dataLen = 0;
  while (*str)
    LMIC.frame[LMIC.dataLen++] = *str++;
  LMIC.osjob.func = func;
  os_radio(RADIO_TX);
  //Serial.println("TX");
}

// Enable rx mode and call func when a packet is received
void rx(osjobcb_t func) {
  LMIC.osjob.func = func;
  LMIC.rxtime = os_getTime(); // RX _now_
  // Enable "continuous" RX (e.g. without a timeout, still stops after
  // receiving a packet)
  os_radio(RADIO_RXON);
  //Serial.println("RX");
}

static void rxtimeout_func(osjob_t *job) {
  lora_digitalWrite(LED_BUILTIN, LOW); // off
}

int buffer_size_size=0;

static void rx_func (osjob_t* job) {
  // Blink once to confirm reception and then keep the led on
  lora_digitalWrite(LED_BUILTIN, LOW); // off
  delay(10);
  lora_digitalWrite(LED_BUILTIN, HIGH); // on

  // Timeout RX (i.e. update led status) after 3 periods without RX
  os_setTimedCallback(&timeoutjob, os_getTime() + ms2osticks(3*TX_INTERVAL), rxtimeout_func);

  // Reschedule TX so that it should not collide with the other side's
  // next TX
  os_setTimedCallback(&txjob, os_getTime() + ms2osticks(TX_INTERVAL/2), tx_func);

  // std::cout << rx_mode << std::endl;
  if(rx_mode=="tele")
  {
    std::cout << LMIC.frame << std::endl;
  } else if(rx_mode=="rx_img")
  {
    std::string lmic_frame_str(reinterpret_cast<char*>(LMIC.frame), 8);
    //std::cout << "lmic data: " << lmic_frame_str << std::endl;
    if(lmic_frame_str == "imgtxend")
    {
      rx_mode="tele";
      std::cout << "-------- end of img tx --------" << std::endl;
      std::cout << "-------- total received array size: " << buffer_size_size << " --------" << std::endl;
      cv::imwrite("r.png", cv::Mat(70,70,CV_8UC1, img_buffer));
      buffer_size_size=0;
    } else
    {
      // std::strcat((char*)img_buffer, (char*)LMIC.frame);
      memcpy(img_buffer+buffer_size_size, LMIC.frame, LMIC.dataLen);
      buffer_size_size += LMIC.dataLen;
      //std::cout << LMIC.frame << std::endl;
      //std::strcat(img_buffer, LMIC.frame);
    }
  }
  // Restart RX
  rx(rx_func);
}

static void txdone_func (osjob_t* job) {
  rx(rx_func);
}

// log text to USART and toggle LED
static void tx_func (osjob_t* job) {
  // say hello
  tx(tx_buffer.c_str(), txdone_func);
  if(tx_buffer=="tx_img")
  {
    rx_mode="rx_img";
  } else if(tx_buffer=="tele")
  {
    rx_mode="tele";
  }
  tx_buffer="";
  // reschedule job every TX_INTERVAL (plus a bit of random to prevent
  // systematic collisions), unless packets are received, then rx_func
  // will reschedule at half this time.
  os_setTimedCallback(job, os_getTime() + ms2osticks(TX_INTERVAL + random(500)), tx_func);
}

// application entry point
void setup() {
  printf("Starting\n");

  lora_pinMode(LED_BUILTIN, OUTPUT);

  // initialize runtime env
  os_init();

  // Set up these settings once, and use them for both TX and RX

#if defined(CFG_eu868)
  // Use a frequency in the g3 which allows 10% duty cycling.
  LMIC.freq = 869525000;
#elif defined(CFG_us915)
  LMIC.freq = 902300000;
#endif

  // Maximum TX power
  LMIC.txpow = 27;
  // Use a medium spread factor. This can be increased up to SF12 for
  // better range, but then the interval should be (significantly)
  // lowered to comply with duty cycle limits as well.
  LMIC.datarate = DR_SF9;
  // This sets CR 4/5, BW125 (except for DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);

  printf("Started\n");

  // setup initial job
  os_setCallback(&txjob, tx_func);
}

void os_alive()
{
  while(1) {
    // execute scheduled jobs and events
    os_runloop_once();
  }
}

int main(void) {
  // initing bcm lib, otherwise it will result in a segmentation fault
  if (!bcm2835_init())
    return 1;

  setup();

  std::thread t1(os_alive);

  while(1)
  {

    std::string cmd;
    std::cout << "Enter CMD: " << std::endl;

    std::getline(std::cin, cmd);
    tx_buffer = cmd;

  }
}
