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
String formTime;
String ipStr;
int counter = 5;
String version ="0.9.6";

// For LEDS
#define LEDC_CHANNEL_0_R  0
#define LEDC_CHANNEL_1_G  1
#define LEDC_CHANNEL_2_B  2
#define LEDC_CHANNEL_3_W  3
// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13
// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000
// LED PINs
#define LED_PIN_R   4
#define LED_PIN_G   19
#define LED_PIN_B   22
#define LED_PIN_W   23

// this delay gives a dimmer time of 10=2.55 seconds 30=7.6 secons 100=25.5 seconds
int delayMills = 20;

// for EEProm
int testvar = 0; // just to check if we must store
uint8_t EEred = 4;
uint8_t EEgreen = 8;
uint8_t EEblue = 12;
uint8_t EEwhite =16;
uint8_t EEset = 20;    // if set to 1 then EEssid and EEwpapw are initialised with one space
uint8_t EEssid = 32;   // eeprom location ssid
uint8_t EEtimer1 = 80;
uint8_t EEtimer2 = 130;
uint8_t EEtimer3 = 180;
uint8_t EEtimer4 = 230;
uint8_t EEtimer5 = 250;



// Object for the wifi connection stored in EEprom 32
struct wifiConn {
  char eSsid[32];
  char ePasw[32];
};
// Objects for timers stored in EEprom 80,130,180,230,250
struct ledTimer {
  int id;
  int startTime;  // minutes since midnight
  int endTime;    
  int red;
  int green;
  int blue;
  int white;
  int status;  //0 = off, 1 = on
  int active;  //0 = off, 1 = on
};
// the colour object
struct colourObj {
  int red;
  int green;
  int blue;
  int white;
};
// init currColour so we can use it global
colourObj currColours;
// also the times should be global
ledTimer runTimes[5];
int maxTimers=5; // DO NOT make larger then 5

bool justOnce = false;
bool debugPrint = true;
bool setupMode = false;
bool setupTimers = false;

// If you are not using MQTT set to false
bool useMQTT = true;        

char const* mqttTopicPublish   = "esp/out";
char const* mqttTopicSubscribe = "esp/in";

// DO NOT EDIT THE VALUE 
const char* mwSk = "dmmadihq7k37samgl4q3m94m1974qidt7tmyalji915ed2j";