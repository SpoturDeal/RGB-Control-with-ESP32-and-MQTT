//variables
const char* ssid     = my_SSID;
const char* password = my_PASSWORD;

const char* assid    = ap_SSID;
const char* asecret  = ap_SECRET;

bool errVal;
const char* datVal;
const char* eepVal;
String respMsg;
String cmdSSID;
String fTime;
int counter = 5;

// For LEDS
#define LEDC_CHANNEL_0_R  0
#define LEDC_CHANNEL_1_G  1
#define LEDC_CHANNEL_2_B  2
#define LEDC_CHANNEL_3_W  3
// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13
// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000
// LED PINs  21 is broken
#define LED_PIN_R   4
#define LED_PIN_G   19
#define LED_PIN_B   22
#define LED_PIN_W   23

uint32_t currRed = 0;
uint32_t currGreen = 0;
uint32_t currBlue = 0;
uint32_t currWhite = 0;
uint32_t readRed = 0;
uint32_t readGreen = 0;
uint32_t readBlue = 0;
uint32_t readWhite = 0;
int delayMills = 10;
// for EEProm
int testvar = 0; // just to check if we must store
uint8_t EEred = 4;
uint8_t EEgreen = 8;
uint8_t EEblue = 12;
uint8_t EEwhite =16;

uint8_t EEssid = 32;   // eeprom location ssid
uint8_t EEset = 20;    // if set to 1 then EEssid and EEwpapw are initialised with one space

struct wifiConn {
  char eSsid[32];
  char ePasw[32];
};

bool justOnce = false;
bool debugPrint = true;
bool setupMode = false;

// DO NOT EDIT THE VALUE
const char* mwSk = "dmmadihq7k37samgl4q3m94m1974qidt7tmyalji915ed2j";