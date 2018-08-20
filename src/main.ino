#include <Arduino.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
#include "credentials.h"
#include "vars.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// Choose the NTP server for your timezone
#define NTP_OFFSET 2 * 60 * 60 // In seconds
#define NTP_INTERVAL 60 * 1000 // In miliseconds
#define NTP_ADDRESS "ntp2.xs4all.nl"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
WiFiServer server(80);

const size_t bufferSize = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(3);
DynamicJsonBuffer jsonBuffer(bufferSize);

//declare functions for C/C++
void setRGBColor(uint32_t R, uint32_t G, uint32_t B, uint32_t W, bool toEEprom);
void setOneColour(uint32_t V, String Clr );
String getValue(String req);
void writeData(uint8_t addr, uint32_t datInt);


void setup() {
  
  // start the EEprom
  EEPROM.begin(128);
  Serial.begin(115200);
  // Start WiFi
  //WiFi.mode(WIFI_STA);
  
  // Start Access point for setting up ssid and password
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  WiFi.softAP(assid,asecret,7,0,5);
  
  

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    WiFi.disconnect();
    ESP.restart();
  }
  
  setupOTA();

  
  initToSerial();
  server.begin();
  timeClient.begin();
  initLEDS();

}



void loop() {
  startColours();
  ArduinoOTA.handle();
  
  
  resetTimer();   // once a day restart ESP
    
  WiFiClient espClient = server.available();        // listen for user by browser
   
  if (espClient) {                             
    
    while (espClient.connected()) {            
      if (espClient.available()) {    // If there is a request from user
        errVal=false;
        eepVal="Not updated";
        respMsg = "OK";     // HTTP Respons Message
        // Read the first line of of the request
        String req = espClient.readStringUntil('\r');
        
        processRequest (req,espClient);  // for commands

        delay(1);
        break;
      }  // espClient.available
    } // espClient.connected
  } // espClient
  delay(1000);
}

String getValue(String req) {

  int val_start = req.indexOf('?');
  if (val_start == -1){
     val_start = req.indexOf('=');
  }
  int val_end   = req.indexOf('/',val_start);
  if (val_end == -1){
     val_end   = req.indexOf(' ',val_start);
  }
  if (val_start == -1 || val_end == -1) {
     if (debugPrint ==true){
        Serial.print("Invalid request: ");
        Serial.println(req);
     }
     return("");
  }
  req = req.substring(val_start + 1, val_end);
  if (debugPrint ==true){
     Serial.print("Requested color: ");
     Serial.println(req);
  }
  return(req);
}
String interfaceUser(){
  String onoff=(currRed + currGreen + currBlue + currWhite > 0?"off":"on");
  String clr = (currRed<16?"0":"")+String(currRed, HEX);
  clr += (currGreen<16?"0":"") +String(currGreen,HEX);
  clr += (currBlue<16?"0":"") +String(currBlue,HEX);
  String ui = "<!DOCTYPE html><html><head><meta name='viewport' content='initial-scale=1.0'><meta charset='utf-8'><style>#map {height: 100%;}html, body {height: 100%;margin: 25px;padding: 10px;font-family: Sans-Serif;} p{font-family:'Courier New', Sans-Serif;}</style>";
  ui += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css\">";
  ui += "<link rel=\"stylesheet\" type=\"text/css\" href=\"https://cdnjs.cloudflare.com/ajax/libs/spectrum/1.8.0/spectrum.min.css\">";
  ui +="<script defer src=\"https://use.fontawesome.com/releases/v5.0.9/js/all.js\"></script>";
  ui +="</head>";
  ui += "<body><h1>RGB(W) control with WiFi</h1>";
  ui += "<button type=\"button\" class=\"btn btn-success btn myBtn\" style=\"display:none;\" id=\"led-on\"><i class=\"far fa-lightbulb fa-2x\"></i>Switch On</button>";
  ui += "<button type=\"button\" class=\"btn btn-secondary btn myBtn\" style=\"display:none;\" id=\"led-off\"><i class=\"fas fa-lightbulb fa-2x\"></i>Switch Off</button><br>";
  ui += "<div> Select a colour.</div>";
  ui += "<div><input type=\"text\" id=\"custom\" /></div>";
  ui += "<div id=\"w\" class=\"alert alert-success\" style=\"margin-top:270px;\" role=\"alert\" style=\"display:none;\"></div>";
  ui += "<div id=\"r\" class=\"alert alert-info\" style=\"margin-top:270px;\" role=\"alert\">"+respMsg+"</div>";
  ui +="<script src=\"https://code.jquery.com/jquery-3.2.1.min.js\"></script><script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js\" ></script><script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js\"></script>";
  ui += "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/spectrum/1.8.0/spectrum.min.js\"></script>";
  ui += "<script>$(document).ready(function($) {";
  ui += "function send(dowhat){$('#r').hide(); $('#w').show(); $.get({url:'/api/command/' + dowhat,dataType:'json',success:function(data){$('#w').hide(); $('#r').html(data.message).show();";
  ui += " }}); } ";
  ui += "$('.myBtn').click(function(){ var per= $(this).attr('id').split('-');$(this).hide();$('#'+per[0]+'-'+(per[1]=='off'?'on':'off')).show(); send(per[1]);  });";
  ui += "$('.myBtn').hide(); $('#led-"+onoff+"' ).show();"; // end buttons
  ui += "$('#custom').spectrum({color:\""+clr+"\",preferredFormat: \"hex\", showInput: true, showPalette: true, ";
  ui += "palette: [[\"#000\",\"#444\",\"#666\",\"#999\",\"#ccc\",\"#eee\",\"#f3f3f3\",\"#fff\"], [\"#f00\",\"#f90\",\"#ff0\",\"#0f0\",\"#0ff\",\"#00f\",\"#90f\",\"#f0f\"], [\"#f4cccc\",\"#fce5cd\",\"#fff2cc\",\"#d9ead3\",\"#d0e0e3\",\"#cfe2f3\",\"#d9d2e9\",\"#ead1dc\"], [\"#ea9999\",\"#f9cb9c\",\"#ffe599\",\"#b6d7a8\",\"#a2c4c9\",\"#9fc5e8\",\"#b4a7d6\",\"#d5a6bd\"], [\"#e06666\",\"#f6b26b\",\"#ffd966\",\"#93c47d\",\"#76a5af\",\"#6fa8dc\",\"#8e7cc3\",\"#c27ba0\"], [\"#c00\",\"#e69138\",\"#f1c232\",\"#6aa84f\",\"#45818e\",\"#3d85c6\",\"#674ea7\",\"#a64d79\"], [\"#900\",\"#b45f06\",\"#bf9000\",\"#38761d\",\"#134f5c\",\"#0b5394\",\"#351c75\",\"#741b47\"], [\"#600\",\"#783f04\",\"#7f6000\",\"#274e13\",\"#0c343d\",\"#073763\",\"#20124d\",\"#4c1130\"]],";
  ui += "change: function(color) { send('hex?' + color.toHex() + '/'); } ";
  ui += " });"; // end of spectrum
  ui += "});"; // end jquery
  ui += "</script></body></html>";
  return ui;         
}
void initLEDS(){
  ledcSetup(LEDC_CHANNEL_0_R, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN_R, LEDC_CHANNEL_0_R);
  
  ledcSetup(LEDC_CHANNEL_1_G, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN_G, LEDC_CHANNEL_1_G);
  
  ledcSetup(LEDC_CHANNEL_2_B, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN_B, LEDC_CHANNEL_2_B);
  
  ledcSetup(LEDC_CHANNEL_3_W, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN_W, LEDC_CHANNEL_3_W);
}
void initToSerial(){
  
  Serial.println();
  Serial.println("Serial started");
  Serial.println("AP started");
  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());
  // Show WiFi is connected
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  Serial.println("Enter this address in your Internet browser.");
}
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // Arduino like analogWrite
  // value has to be between 0 and valueMax
  // calculate duty
  uint32_t duty = (LEDC_BASE_FREQ / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(channel, duty);
}
uint32_t min(uint32_t num1, uint32_t num2){
  if(num1 < num2){
    return num1;
  }else{
    return num2;
  }
}
void processRequest(String req, WiFiClient espClient){
        // find the commands it always starts with command
        
        // http://{ip-address}/command/..../ 

        // if you use without browser for home automation or domoticz
        // api is needed in the request (no need for interface)

        // http://{ip-address}/api/command/..../ 
        
        // this will result in a reply only
        // al requests must end with an forward slash /
        if (req.indexOf("/command/on") != -1) {
           // switch all on
           EEPROM.get(EEred,currRed);
           EEPROM.get(EEgreen,currGreen);
           EEPROM.get(EEblue,currBlue);
           EEPROM.get(EEwhite,currWhite);
           ledcAnalogWrite(LEDC_CHANNEL_0_R, currRed);
           ledcAnalogWrite(LEDC_CHANNEL_1_G, currGreen);
           ledcAnalogWrite(LEDC_CHANNEL_2_B, currBlue);
           ledcAnalogWrite(LEDC_CHANNEL_3_W, currWhite);
           respMsg = "LEDS returned to old state ";
        } else if (req.indexOf("/command/off") != -1) {
           // switch all off 
           ledcAnalogWrite(LEDC_CHANNEL_0_R, 0);
           ledcAnalogWrite(LEDC_CHANNEL_1_G, 0);
           ledcAnalogWrite(LEDC_CHANNEL_2_B, 0);
           ledcAnalogWrite(LEDC_CHANNEL_3_W, 0);
           respMsg = "LEDS switched off will restart in previous state";
        } else if (req.indexOf("/command/red") != -1) {
           // set color RED  example command/red?128/
           int recvVal = getValue(req).toInt();
           if (testRecvVal(recvVal)) {
              setOneColour(recvVal, "red");
              respMsg = "LED Red set to: "+(String)recvVal;
           }   
        } else if (req.indexOf("/command/green") != -1) {
           // set color GREEN example command/green?64/
           int recvVal = getValue(req).toInt();
           if (testRecvVal(recvVal)) {
              setOneColour(recvVal, "green");
              respMsg = "LED Green set to: "+(String)recvVal;
           }
        } else if (req.indexOf("/command/blue") != -1) {
           // set color BLUE example command/blue?32/
           int recvVal = getValue(req).toInt();
           if (testRecvVal(recvVal)) {
              setOneColour(recvVal, "blue");
              respMsg = "LED Blue set to: "+(String)recvVal;
           }   
        } else if (req.indexOf("/command/white") != -1) {
           // set color WHITE example command/white?16/
           int recvVal = getValue(req).toInt();
           if (testRecvVal(recvVal)) {
              setOneColour(recvVal, "white");
              respMsg = "LED White set to: "+(String)recvVal;
           }
        } else if (req.indexOf("/command/set") != -1) {
            // set the colour using rgb example set?255,128,64,32/
            // if you don't have a RGBW but a RGB then ommit the fourt number
            String recvVal = getValue(req);
            // Break the string into colours
            int pSep1 = recvVal.indexOf(',');
            int Red = recvVal.substring(0 , pSep1).toInt();
            
            int pSep2 = recvVal.indexOf(',',pSep1 + 1);
            int Green = recvVal.substring(pSep1 + 1,pSep2).toInt();
            
            int pSep3 = recvVal.indexOf(',', pSep2 + 1);
            int Blue = recvVal.substring(pSep2 + 1, pSep3).toInt();

            int pSepEnd = recvVal.length();
            int White = recvVal.substring(pSep3 + 1 , pSepEnd).toInt();
            
            // Check if the values are valid otherwise adjust to limits
            if (Red < 0) {Red = 0;}
            if (Green < 0) {Green = 0;}
            if (Blue < 0) {Blue = 0;}
            if (White < 0) {White = 0;}

            if (Red > 255) {Red = 255;}
            if (Green > 255) {Green = 255;}
            if (Blue > 255) {Blue = 255;}
            if (White > 255) {White = 255;}

            setRGBColor(Red,Green,Blue,White,true);
            respMsg = "OK Colours set to Red: "+(String)Red+" Green: "+(String)Green+" Blue: "+(String)Blue+" White: "+(String)White;
        } else if (req.indexOf("/command/hex") != -1) {
            // set the colour in hex example hex?FFEEDDCC/ or hex?FFEEDD/
            String recvVal = getValue(req);
            int n = recvVal.length();
            if (n ==6 || n==8){
              // convert string to char
              char const *str;
              str = recvVal.c_str();
              // define the colour chars
              char cRed[5] = {0};
              char cGreen[5] = {0};
              char cBlue[5] = {0};
              char cWhite[5] = {0};
              // fill the colour chars
              cRed[0] = cGreen[0] = cBlue[0] = cWhite[0] = '0';
              cRed[1] = cGreen[1] = cBlue[1] = cWhite[1] = 'X';

              cRed[2] = str[0];
              cRed[3] = str[1];
              cGreen[2] = str[2];
              cGreen[3] = str[3];
              cBlue[2] = str[4];
              cBlue[3] = str[5];
              // if white is used
              if (n == 8){
                 cWhite[2] = str[6];
                 cWhite[3] = str[7];
              }
              // now convert the hex numbers to integers
              int Red = strtol(cRed, NULL, 16);
              int Green = strtol(cGreen, NULL, 16);
              int Blue = strtol(cBlue, NULL, 16);
              int White = 0;
              if (n == 8){
                 White = strtol(cWhite, NULL, 16);
              }
              setRGBColor(Red,Green,Blue,White,true);
              respMsg = "Colours set to Red: "+(String)Red+" Green: "+(String)Green+" Blue: "+(String)Blue+" White: "+(String)White;
            } else {
              errVal = true;
              respMsg = "Invalid length HEX code possible FFEEAA or FFEEAABB";
            }
           
        }
        //make the user interface
        String s = "Something went wrong with web interface";
        if (req.indexOf("/api/") == -1) {
          s = interfaceUser();


           // send reply
           espClient.print(s);
        } else if (req.indexOf("/api/") != -1){
          //  If used from api just a line reply
          datVal =  respMsg.c_str();
          JsonObject& root = jsonBuffer.createObject();
          root["error"] = errVal;
          root["status"] = (errVal==true?"Error occured, check your request":"OK");
          root["time"] = fTime; 
          root["message"] = datVal;
          root["eeprom"] = eepVal;
          root.printTo(Serial);
          Serial.print("POST ");
          root.printTo(espClient);
          
        } else {
          espClient.print(s);
        } 
}
void resetTimer(){
  timeClient.update();
  String fTime = timeClient.getFormattedTime();
  if (fTime.substring(0,2).toInt()==1 &&
      fTime.substring(3,5).toInt() % 53 == 0 && 
      fTime.substring(6,8).toInt()<3 ){
      ESP.restart();
  }  
}
void setOneColour(uint32_t V, String Clr ){
    // set var for default
    int Pin = LED_PIN_W;
    int currTemp = currWhite;
    int Channel = LEDC_CHANNEL_3_W;
    // set the proper pin and current colour
    if (Clr == "red"){
       Pin = LED_PIN_R; 
       currTemp = currRed;
       Channel = LEDC_CHANNEL_0_R; 
    } else if (Clr == "green"){
       Pin = LED_PIN_G; 
       currTemp = currGreen;
       Channel = LEDC_CHANNEL_1_G; 
    } else if (Clr == "blue"){
       Pin = LED_PIN_B; 
       currTemp = currBlue; 
       Channel = LEDC_CHANNEL_2_B;
    } 
    ledcSetup(Channel, LEDC_BASE_FREQ ,LEDC_TIMER_13_BIT);
    ledcAttachPin(Pin, Channel);
    for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++) {
      // Increase! Cycle up loop and check what is current level
      if ( V > dutyCycle && V > currTemp ){
        ledcAnalogWrite(Channel, dutyCycle);
      }
      if ( V < 255-dutyCycle && V < currTemp){
        ledcAnalogWrite(Channel, 255-dutyCycle);
      }
      delay(delayMills);
    }
    //update the current colour and write to eeprom
    if (Clr == "red"){
       currRed = V; 
       writeData(EEred, V);
    } else if (Clr == "green"){
       currGreen = V; 
       writeData(EEgreen, V);
    } else if (Clr == "blue"){
       currBlue = V; 
       writeData(EEblue, V);
    } else if (Clr == "white"){
       currWhite = V; 
       writeData(EEwhite, V);
    } 
   
}
void setRGBColor(uint32_t R, uint32_t G, uint32_t B, uint32_t W, bool toEEprom) {
    // all colours are called in each cycle to make all leds
    // increase or decrease at the same moment.
    // Getting more light first is to prevent flashes 
    for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++) {
      if ( R > dutyCycle && R > currRed ){
        ledcAnalogWrite(LEDC_CHANNEL_0_R, dutyCycle);
      }
      if ( G > dutyCycle && G > currGreen ){
        ledcAnalogWrite(LEDC_CHANNEL_1_G, dutyCycle);
      }
      if ( B > dutyCycle && B > currBlue ){
        ledcAnalogWrite(LEDC_CHANNEL_2_B, dutyCycle);
      }
      if ( W > dutyCycle && W > currWhite ){
        ledcAnalogWrite(LEDC_CHANNEL_3_W, dutyCycle);
      }
      if ( R < 255-dutyCycle && R < currRed ){
        ledcAnalogWrite(LEDC_CHANNEL_0_R, 255-dutyCycle);
      }
      if ( G < 255-dutyCycle && G < currGreen ){
        ledcAnalogWrite(LEDC_CHANNEL_1_G, 255-dutyCycle);
      }
      if ( B < 255-dutyCycle && B < currBlue ){
        ledcAnalogWrite(LEDC_CHANNEL_2_B, 255-dutyCycle);
      }
      if ( W < 255-dutyCycle && W < currWhite ){
        ledcAnalogWrite(LEDC_CHANNEL_3_W, 255-dutyCycle);
      }
      // the delay can't be too large it would stop the loop
      // 10 is about 2.5 second (255 * 10)
      delay (delayMills);
      Serial.println("");
    }
    
  
  Serial.println("Red: "+(String)R+" Green: "+(String)G+" Blue: "+(String)B+" White: "+(String)W);
  // store the last set colours and store in eeprom
  currRed = R;
  currGreen = G;
  currBlue = B;
  currWhite = W;
  if (toEEprom==true){
    writeData(EEred, currRed);
    writeData(EEgreen, currGreen);
    writeData(EEblue, currBlue);
    writeData(EEwhite, currWhite);
  }
}
void setupOTA(){
  // Set up Over The Air updates
  ArduinoOTA.setHostname("RGBW_driver");
  //ArduinoOTA.setPassword(my_OTA_PW);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}
void startColours(){
  if (justOnce == false){
    ledcAnalogWrite(LEDC_CHANNEL_0_R, 0);
    ledcAnalogWrite(LEDC_CHANNEL_1_G, 0);
    ledcAnalogWrite(LEDC_CHANNEL_2_B, 0);
    ledcAnalogWrite(LEDC_CHANNEL_3_W, 0);
    // restore to latest setting after restart
    // retrieve data from EEprom if available
    EEPROM.get(EEred,currRed);
    EEPROM.get(EEgreen,currGreen);
    EEPROM.get(EEblue,currBlue);
    EEPROM.get(EEwhite,currWhite);
    ledcAnalogWrite(LEDC_CHANNEL_0_R, currRed);
    ledcAnalogWrite(LEDC_CHANNEL_1_G, currGreen);
    ledcAnalogWrite(LEDC_CHANNEL_2_B, currBlue);
    ledcAnalogWrite(LEDC_CHANNEL_3_W, currWhite);
    Serial.println("first colours set");
    justOnce = true;
  }
}
bool testRecvVal(int rVal){
   if ( rVal >= 0 && rVal <= 255 ){
      return true;
   } else {
      errVal = true;
      respMsg = "colour value out of range";
      return false;
   }
}
void writeData(uint8_t addr, uint32_t datInt){
    int testVar = 0;
    // to conserve flash memory only write when differs
    
    EEPROM.get(addr,testVar);
    String tcl="White";
    if (addr == 4){
      tcl="Red  ";
    } else if (addr == 8){
      tcl="Green";
    } else if (addr == 12){
      tcl="Blue ";
    }
    // check the data against the read data.
    if (datInt != testVar){
       // here put the data in EEprom
       EEPROM.put(addr,datInt);
       EEPROM.commit();
       if (debugPrint ==true){
          
          Serial.print("-- Updated EEPROM "+tcl+" to: ");
          eepVal="Data has been updated";
          testVar = 0; 
          // Get the data to be sure it is new.
          EEPROM.get(addr,testVar);
          Serial.println(testVar);
       }   
    } else {
       Serial.println("EEPROM "+tcl+" not updated because value "+String(datInt)+" didn't change");
    }  
}