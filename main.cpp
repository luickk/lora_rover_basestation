#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <sstream>
#include <limits>
#include <set>
#include <lmic.h>
#include <hal/hal.h>
#include <boost/asio.hpp>

#include "libs/MicroNMEA/MicroNMEA.cpp"

#include <wiringPi.h>
#include <wiringSerial.h>

std::string tx_io_buf = "";
std::string rx_io_buf = "";

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

// Create a server endpoint

typedef websocketpp::server<websocketpp::config::asio> server;

server echo_server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

std::vector<websocketpp::connection_hdl> hdl_list;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

int con_count = 0;
// Define a callback to handle incoming messages
void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
  con_count++;
  hdl_list.push_back(hdl);
  tx_io_buf = msg->get_payload();
  std::cout << "wsock received: " << msg->get_payload() << std::endl;

  if (msg->get_payload() == "stop-listening") {
      s->stop_listening();
      return;
  }
}
void on_open(websocketpp::connection_hdl hdl) {
  std::cout << "con opened" << std::endl;
}

void on_close(websocketpp::connection_hdl hdl) {
}

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

bool isASCII (const std::string& s)
{
    return !std::any_of(s.begin(), s.end(), [](char c) {
        return static_cast<unsigned char>(c) > 127;
    });
}

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

int i = 0;
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

  Serial.print("Got ");
  Serial.print(LMIC.dataLen);
  Serial.println(" bytes");
  Serial.write(LMIC.frame, LMIC.dataLen);
  Serial.println();

  std::string buf(reinterpret_cast< char const* >(LMIC.frame));
  if(isASCII(buf))
  {
    if(con_count>0)
    {
      for(std::vector<int>::size_type i = 0; i != hdl_list.size(); i++)
      {
        std::cout << "forwarding data" << std::endl;
        echo_server.send(hdl_list[i], buf,  websocketpp::frame::opcode::text);
      }
    }
    }
  // Restart RX
  rx(rx_func);
}

static void txdone_func (osjob_t* job)
{
  rx(rx_func);
}

// log text to USART and toggle LED
static void tx_func (osjob_t* job) {
  // say hello
  if(isASCII(tx_io_buf))
  {
    tx(tx_io_buf.c_str(), txdone_func);
    // reschedule job every TX_INTERVAL (plus a bit of random to prevent
    // systematic collisions), unless packets are received, then rx_func
    // will reschedule at half this time.
  } else
  {
    std::cout << "skipped forwarding because it contained non ascii characters" << std::endl;
  }
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

void lora_keep_alive()
{
  while(1) {
    // execute scheduled jobs and events
    os_runloop_once();
  }
}

void gps_serial_thread()
{
  std::cout << "gps thread started" << std::endl;

  int fd = 0;
  REINIT:if ((fd = serialOpen ("/dev/ttyS0", 9600)) < 0)
  {
   fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno)) ;
  }

  //MicroNMEA library structures
  char nmeaBuffer[200];
  MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));

  std::string data;
  std::string NavSystem;
  int NumSatellites;
  long lat;
  long lon;

  double latf;
  double lonf;

  int input = 0;

  while (serialDataAvail (fd))
  {
    input = serialGetchar (fd);
    try
    {
      nmea.process(input);

      NumSatellites = nmea.getNumSatellites();

      lat = nmea.getLatitude();
      lon = nmea.getLongitude();

      latf = (double) lat/1000000;
      lonf = (double) lon/1000000;

      float debug_lat = 49.466602;
      float debug_lon = 10.967921;

      std::cout << "Lat: " << latf << ", Lon: " << lonf << std::endl;


    }
    catch (const std::invalid_argument& ia) {
        //std::cerr << "Invalid argument: " << ia.what() << std::endl;
    }
    catch (const std::out_of_range& oor) {
        //std::cerr << "Out of Range error: " << oor.what() << std::endl;
    }
    catch (const std::exception& e)
    {
        //std::cerr << "Undefined error: " << e.what() << std::endl;
    }

    usleep(10000);

    if(input==-1){
      goto REINIT;
    }
  }
}

int main(void)
{
  // initing bcm lib, otherwise it will result in a segmentation fault
  if (!bcm2835_init())
    return 1;

  setup();

  /*
  Todo start gps listening thread
  */
  std::thread gps_serial(gps_serial_thread);

  std::thread os_(lora_keep_alive);

  try {
      boost::asio::ip::tcp::acceptor::reuse_address(true);

      // Set logging settings
      // echo_server.set_access_channels(websocketpp::log::alevel::all);
      echo_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

      // Initialize Asio
      echo_server.init_asio();

      // Register our message handler
      echo_server.set_message_handler(bind(&on_message,&echo_server,::_1,::_2));

      // echo_server.set_close_handler(bind(&on_close,&echo_server,::_1,));


      // Listen on port 9002
      echo_server.listen(9002);

      // Start the server accept loop
      echo_server.start_accept();

      // Start the ASIO io_service run loop
      echo_server.run();

  } catch (websocketpp::exception const & e) {
      std::cout << e.what() << std::endl;
  } catch (...) {
      std::cout << "other exception" << std::endl;
  }

}
