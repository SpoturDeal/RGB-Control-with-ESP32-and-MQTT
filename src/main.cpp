#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "credentials.h"
#include <cstring>
#include <cstdlib>
#include <EEPROM.h>
#include <ESPmDNS.h>
#include <iostream>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <string>
#include "vars.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// Choose the NTP server for your timezone
#define NTP_OFFSET 2 * 60 * 60 // In seconds
#define NTP_INTERVAL 60 * 1000 // In miliseconds
#define NTP_ADDRESS "ntp2.xs4all.nl"
// we need bigger packets.
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 1024
#endif

// Time client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
//WiFi server
WiFiServer server(80);
WiFiClient espClient;
// MQTT Client
PubSubClient mqttClient(espClient);

// Buffers for JSON
const size_t bufferSize = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(3);
DynamicJsonBuffer jsonBuffer(bufferSize);

//declare functions for C/C++
void callback(char* topic, byte* payload, unsigned int length);
colourObj fillColourObj(int red, int green, int blue, int white);
String getValue(String req);
unsigned char h2int(char c);
void initLEDS();
void initToSerial();
void interfaceUser(WiFiClient espClient);
void interfaceSetUp(WiFiClient espClient);
void interfaceTimers(WiFiClient espClient);
void interfaceTimerRow(WiFiClient espClient,int n);
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255);
uint32_t min(uint32_t num1, uint32_t num2);
void processRequest(String req, WiFiClient espClient);
void publishToMQTT();
void reconnect_mqtt();
String recvVar(String req, String base, String nm);
void resetTimer();
void scanNetworks();
void setOneColour(uint32_t V, String Clr );
void setRGBColor(colourObj rC, bool toEEprom);
void setupOTA();
void startColours();
bool testRecvVal(int rVal);
String urldecode(String str);
String urlencode(String str);
void writeData(uint8_t addr, uint32_t datInt);
void write_timer_toEEPROM(ledTimer timedat);
ledTimer read_timer_fromEEPROM(int id);
void write_wifi_toEEPROM(uint8_t startAddr, String strSSID, String strPW);
wifiConn read_wifi_fromEEPROM(uint8_t startAddr);

void setup() {
  // start the EEprom
  EEPROM.begin(640);
  Serial.begin(115200);
  EEPROM.get(EEset,testvar);
  if (testvar -= 1){
    // fill the ssid on eeprom with a fixed nones
    //if not set store dummy data in EEprom
    write_wifi_toEEPROM(EEssid, "nonessid", "nonepassword");
    writeData(EEset,1);  // is only set one time
    //Dummy data timers
    ledTimer timedat;
    for (int i = 0; i < maxTimers; i++){
      timedat = { i, 0, 0, 0, 0, 0, 0, 0, 0 };
      write_timer_toEEPROM(timedat);
    }
  }
  // load the timers
  for (int i = 0; i < maxTimers; i++){
     runTimes[i] = read_timer_fromEEPROM(i);
  }
  // if no ssid and password in credentials.h then check eeprom
  if (strlen(ssid) < 3 || strlen(password) < 3){
      //if was set then read the data 
      wifiConn staConn = read_wifi_fromEEPROM(EEssid);
      //use the data from EEprom if it is not nones
      if ( strcmp(staConn.eSsid,"nonessid") !=0 ){
        ssid = staConn.eSsid; 
      }
      if ( strcmp(staConn.ePasw, "nonepassword") != 0) { 
        password = staConn.ePasw; 
      }
    
  }
  // Start WiFi depending on mode
  // Start Access point if ssid is not set
  // for setting up ssid and password
  if (strlen(ssid) < 3 || strlen(password) < 3){
     WiFi.mode(WIFI_AP);
     WiFi.softAP(assid,asecret,7,0,5);
     WiFi.begin(ssid, password);
     setupMode = true;
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Set to AP mode...");
      delay(5000);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(assid,asecret,7,0,5);
      setupMode = true;
      break;
    }
  } 
  // in setMode we need different important parts
  scanNetworks();
  if (setupMode == false){
    setupOTA();
    // Setup and start MQTT is useMQTT in vars.h is true
    if (useMQTT == true){
      mqttClient.setServer(ip_MQTT, port_MQTT);
      while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT ... ");
        if (mqttClient.connect("ESP32Client", my_MQTT_USER, my_MQTT_PW )) {
          Serial.println("Connected to MQTT");
          mqttClient.setCallback(callback);
          mqttClient.subscribe(mqttTopicSubscribe);
          mqttClient.publish(mqttTopicPublish, "ESP32 RGB has just been started.");
          // remove all retained messages to prevent flooding with the same message
          // mqttClient.publish("esp/in", new byte[0],0,true);
        } else {
          Serial.print("Failed to connect to MQTT with state ");
          Serial.print(mqttClient.state());
          delay(2000);
        }
      } // end mqttclientconnected
    } // end do we use MQTT
  } // end setupMode
  // write some startup info
  initToSerial();
  // start the server
 
  server.begin();
  // start the time client
  timeClient.begin();
  // setup pins for LEDS
  initLEDS();
  // make an url/ip string that we understand for userface redirects
  IPAddress myIP = WiFi.localIP();
  ipStr = String(myIP[0])+"."+String(myIP[1])+"."+String(myIP[2])+"."+String(myIP[3]); 
}

void loop() {
  // only needed when running not for setup
  if (setupMode == false){
      // LEDS to last setting
      startColours();
      ArduinoOTA.handle();
      // Make sure MQTT is still connected
      if (useMQTT == true){
        mqttClient.loop();
        // make sure MQTT stays connected
        if (!mqttClient.connected()) {
          reconnect_mqtt();
        }
      }
  }
  // once a day restart ESP
  resetTimer();
  int currMinute = timeClient.getHours() * 60 + timeClient.getMinutes();
  // check if a timer needs to be activated
  for (int tmCheck = 0; tmCheck < maxTimers; tmCheck++){
     if (runTimes[tmCheck].active == 1 ){
        colourObj tmC;
        if (runTimes[tmCheck].startTime == currMinute && runTimes[tmCheck].status == 0 ){
            // set the requested colour based on timer
            tmC = fillColourObj(runTimes[tmCheck].red,runTimes[tmCheck].green,runTimes[tmCheck].blue,runTimes[tmCheck].white);
            setRGBColor(tmC,true);
            // set the status to 1 to prevent multiple start in the minute
            runTimes[tmCheck].status=1;
        } else if (runTimes[tmCheck].endTime == currMinute && runTimes[tmCheck].status == 0 ){
            tmC = fillColourObj(0,0,0,0);
            setRGBColor(tmC,false);
             // set the status to 0 to prevent multiple sops in the minute
            runTimes[tmCheck].status=1;
        }  
		if ((runTimes[tmCheck].startTime != currMinute && 
             runTimes[tmCheck].endTime != currMinute) && 
             runTimes[tmCheck].status == 1 ){
             // reset the status after the currMinute  
             runTimes[tmCheck].status = 0;
        }
     }
  }

  WiFiClient espClient = server.available();        // listen for user by browser
  if (espClient) {                             
    
    while (espClient.connected()) {            
      if (espClient.available()) {    // If there is a request from user
        errVal = false;
        eepVal = "Not updated";
        respMsg = "Ready";     // HTTP Respons Message
        // Read the first line of of the request
        String req = espClient.readStringUntil('\r');
        setupTimers = false; // can change in processrequest
        processRequest (req,espClient);  // for commands
        String s = "Something went wrong with web interface";
        if (req.indexOf("/api/") == -1) {
            if (setupMode == false){
                if (setupTimers == false){
                  interfaceUser(espClient);
                } else {
                  interfaceTimers(espClient);
                }
                break;
            } else {
                interfaceSetUp(espClient);
            }
            break;
        } else if (req.indexOf("/api/") != -1){
            //  Make a JSON reply that the receiver can handle
            if (setupMode == false){
              datVal =  respMsg.c_str();
              JsonObject& root = jsonBuffer.createObject();
              root["error"] = errVal;
              root["status"] = (errVal==true?"Error occured, check your request":"OK");
              root["message"] = datVal;
              JsonObject& colours = root.createNestedObject("colours");
              colours["red"]=currColours.red;
              colours["green"]=currColours.green;
              colours["blue"]=currColours.blue;
              colours["white"]=currColours.white;
              JsonObject& updated = root.createNestedObject("updated");
              updated["time"] = formTime;
              updated["eeprom"] = eepVal;
              updated["version"] = version;
              root.printTo(espClient);
              // must stop otherwise MQTT is not sent
              espClient.stop();
              delay(500); 
            } else {
              espClient.print(respMsg);
            }
        } else {
            // a request gives just a reply
            if (setupTimers == false){
              espClient.print(s);
            } 
        } 
        espClient.stop();
        delay(1);
        break;
      }  // espClient.available
    } // espClient.connected
  } // espClient
  delay(1000);
}
void callback(char* topic, byte* payload, unsigned int length) {
    
  Serial.println("-------new message from MQTT broker-----");
  Serial.print("channel: ");
  Serial.println(topic);
  Serial.print("data: ");  
  Serial.write(payload, length);
  Serial.print(" length: ");
  Serial.println(length);
  // Read the payload and see if it is a JSON object
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& message = jsonBuffer.parseObject((char *)payload);
  if (!message.success()) {
      Serial.println("MQTT received no JSON Object here is the message.");
      for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
      }
      Serial.println();
      Serial.println("-----------------------");  
      return;
  }
  // payload.colours is the request to control by MQTT
  JsonArray& colours=message["payload"]["colours"];
  if (colours.size() > 0){
    colourObj tmC = fillColourObj(colours[0]["red"],colours[0]["green"],colours[0]["blue"],colours[0]["white"]);
    setRGBColor(tmC,true);
  }
    
  
}
colourObj fillColourObj(int red, int green, int blue, int white){
    colourObj vars;
    vars.red = red;
    vars.green = green;
    vars.blue = blue;
    vars.white = white;
    return vars;
}
String getValue(String req) {
  // get the value of the request (may need changes later)
  int val_start = req.indexOf('?');
  if (val_start == -1){
     val_start = req.indexOf('=');
  }
  int val_end   = req.indexOf('/',val_start);
  if (val_end == -1){
     val_end   = req.indexOf(' ',val_start);
  }
  if (val_start == -1 || val_end == -1) {
     if (debugPrint == true){
        Serial.print("Invalid request: ");
        Serial.println(req);
     }
     return("");
  }
  req = req.substring(val_start + 1, val_end);
  if (debugPrint == true){
     Serial.print("Requested color: ");
     Serial.println(req);
  }
  return(req);
}
unsigned char h2int(char c){
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}
void interfaceUser(WiFiClient espClient) {
  // build and send the user interface
  String onoff=(currColours.red + currColours.green + currColours.blue + currColours.white > 0?"off":"on");
  String clr = (currColours.red<16?"0":"")+String(currColours.red, HEX);
  clr += (currColours.green<16?"0":"") +String(currColours.green,HEX);
  clr += (currColours.blue<16?"0":"") +String(currColours.blue,HEX);

  espClient.print ("<!DOCTYPE html><html><head><meta name='viewport' content='initial-scale=1.0'><meta charset='utf-8'>");
  espClient.print ("<style>#map {height: 100%;} html, body {height: 100%; margin: 25px; padding: 10px;font-family: Sans-Serif;} #container{max-width:600px;} #footer{position: absolute;bottom: 0; font-size:8px;}</style>");
  espClient.print ("<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css'>");
  espClient.print ("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/spectrum/1.8.0/spectrum.min.css'>");
  espClient.print ("<script defer src='https://use.fontawesome.com/releases/v5.0.9/js/all.js'></script>");
  espClient.print ("</head>");
  espClient.print ("<body><div id='container'><h1>RGBw control with WiFi</h1>");
  espClient.print ("<button type='button' class='btn btn-success btn myBtn' style='display:none;' id='led-on'><i class='far fa-lightbulb fa-2x'></i>Switch On</button>");
  espClient.print ("<button type='button' class='btn btn-secondary btn myBtn' style='display:none;' id='led-off'><i class='fas fa-lightbulb fa-2x'></i>Switch Off</button><br>");
  espClient.print ("<div>Move slider to change colour</div>");
  espClient.print ("<div><table><tr><td>Red</td><td><input id='red' class='range' type='range' min='0' max='255' value='"+(String)currColours.red+"'></td><td style='text-align: right;'><span id='vred'>"+(String)currColours.red+"</span></td></tr>");
  espClient.print ("<tr><td>Green</td><td><input id='green' class='range' type='range' min='0' max='255' value='"+(String)currColours.green+"'></td><td style='text-align:right;'><span id='vgreen'>"+(String)currColours.green+"</span></td></tr>");
  espClient.print ("<tr><td>Blue</td><td><input id='blue' class='range' type='range' min='0' max='255' value='"+(String)currColours.blue+"'></td><td style='text-align:right;'><span id='vblue'>"+(String)currColours.blue+"</span></td></tr>");
  espClient.print ("<tr><td>White</td><td><input id='white'class='range' type='range' min='0' max='255' value='"+(String)currColours.white+"'></td><td style='text-align:right;'><span id='vwhite'>"+(String)currColours.white+"</span></td></tr></table><div>");
  espClient.print ("<div>Or select a colour with the colour picker.</div>");
  espClient.print ("<div><input type='text' id='custom' /></div>");
  espClient.print ("<div id='w' class='alert alert-info' role='alert'> " + respMsg + "</div>");
  espClient.print ("<div id='footer' ><button type='button' id='wifi' class='btn btn-outline-secondary btn-sm'><i class='fas fa-wifi'></i>&nbsp;Change WiFi</button> ");
  espClient.print ("<button type='button' id='timers' class='btn btn-outline-secondary btn-sm'><i class='fas fa-clock'></i>&nbsp;Timers</button>"); 
  espClient.print ("<span>&nbsp;Version: " + version + "</span>");
  espClient.print ("</div></div>");
  espClient.print ("<script src='https://code.jquery.com/jquery-3.2.1.min.js'></script><script src='https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js' ></script><script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js'></script>");
  espClient.print ("<script src='https://cdnjs.cloudflare.com/ajax/libs/spectrum/1.8.0/spectrum.min.js'></script>");
  espClient.print ("<script>");
  espClient.println ("$(document).ready(function($) {");
  espClient.println ("  function send(dowhat){"); 
  espClient.println ("    $('#w').html('Please wait until action has finished. ');");
  espClient.println ("    $('.range').prop('disabled', true);");
  espClient.println ("    $.get({url:'/api/command/' + dowhat,"); // send the request using AJAX
  espClient.println ("      dataType:'json',");
  espClient.println ("       success:function(data){");
  espClient.println ("          $('#w').html(data.message);");
  espClient.println ("          $('.range').prop('disabled', false);");
  espClient.println ("          var c = data.colours;");
  espClient.println ("          $('#red').val(c.red);");      // update 4 sliders
  espClient.println ("          $('#green').val(c.green);");
  espClient.println ("          $('#blue').val(c.blue);");
  espClient.println ("          $('#white').val(c.white);");
  espClient.println ("           $('#vred').html(c.red);");    // udate the number behind slider
  espClient.println ("          $('#vgreen').html(c.green);");
  espClient.println ("          $('#vblue').html(c.blue);");
  espClient.println ("          $('#vwhite').html(c.white);");
  espClient.println ("          var hexString = (c.red<16?'0':'')+ c.red.toString(16);"); // make hex
  espClient.println ("          hexString += (c.green<16?'0':'')+ c.green.toString(16);");
  espClient.println ("          hexString += (c.blue<16?'0':'')+ c.blue.toString(16);");
  espClient.println ("          $('#custom').spectrum('set',hexString);");  // set the colour to picker
  espClient.println ("       }");
  espClient.println ("    });");
  espClient.println ("  } ");
  espClient.println ("  $('.range').change(function(){"); // react to change of colour slider
  espClient.println ("    send($(this).attr('id')+'='+$(this).val());"); // prepare to send
  espClient.println ("    $('#v'+$(this).attr('id')).html($(this).val());");
  espClient.println ("  });");
  espClient.println ("  $('.myBtn').click(function(){"); // react to click button on/off
  espClient.println ("    var per= $(this).attr('id').split('-');"); // id of btn
  espClient.println ("    $(this).hide();");
  espClient.println ("    $('#'+per[0]+'-'+(per[1]=='off'?'on':'off')).show();");
  espClient.println ("    send(per[1]);"); // prepare to send 
  espClient.println ("  });");
  espClient.println ("  $('#wifi').click(function(){ ");
  espClient.println ("      window.location.href = 'http://" + ipStr + "/command/wifi';");
  espClient.println ("  });");
  espClient.println ("  $('#timers').click(function(){");
  espClient.println ("      window.location.href = 'http://" + ipStr + "/settimers';");
  espClient.println ("  });");
  espClient.println ("  $('.myBtn').hide();"); // all buttons hide 
  espClient.println ("  $('#led-"+onoff+"' ).show();"); // only show button that is needed
  espClient.println ("  $('#custom').spectrum({");  // color picker by bgrin https://bgrins.github.io/spectrum/
  espClient.println ("    color:'"+clr+"',");
  espClient.println ("    preferredFormat: 'hex',");
  espClient.println ("    showInput: true,");
  espClient.println ("    showPalette: true,");
  espClient.println ("     hideAfterPaletteSelect:true, ");
  espClient.println ("    palette: [['#000','#444','#666','#999','#ccc','#eee','#f3f3f3','#fff'], ['#f00','#f90','#ff0','#0f0','#0ff','#00f','#90f','#f0f'], ['#f4cccc','#fce5cd','#fff2cc','#d9ead3','#d0e0e3','#cfe2f3','#d9d2e9','#ead1dc'], ['#ea9999','#f9cb9c','#ffe599','#b6d7a8','#a2c4c9','#9fc5e8','#b4a7d6','#d5a6bd'], ['#e06666','#f6b26b','#ffd966','#93c47d','#76a5af','#6fa8dc','#8e7cc3','#c27ba0'], ['#c00','#e69138','#f1c232','#6aa84f','#45818e','#3d85c6','#674ea7','#a64d79'], ['#900','#b45f06','#bf9000','#38761d','#134f5c','#0b5394','#351c75','#741b47'], ['#600','#783f04','#7f6000','#274e13','#0c343d','#073763','#20124d','#4c1130']],");
  espClient.println ("    change: function(color) {");
  espClient.println ("      send('hex?' + color.toHex() + '/');");
  espClient.println ("    } ");
  espClient.println ("  });"); // end of spectrum
  espClient.println ("});"); // end jquery
  espClient.print ("</script></body></html>");
  espClient.stop();     
}
void interfaceSetUp(WiFiClient espClient){
  // Build the wifi setup interface
  espClient.print ("<!DOCTYPE html><html><head><meta name='viewport' content='initial-scale=1.0'><meta charset='utf-8'>");
  espClient.print ("<style>#map {height: 100%;} html, body {height: 100%; margin: 25px; padding: 10px; font-family: Sans-Serif;} #container{max-width:600px;} button, input, select {margin:15px 0px} #footer{position: absolute;bottom: 0;}</style>");
  espClient.print ("</head>");
  espClient.print ("<body><div id='container'><h1>RGBw control setup WiFi</h1>");
  espClient.print ("<div>Select your Wifi Station<br>");
  espClient.print (cmdSSID);  // this are the scanned networks in a html select
  espClient.print ("</div><div>Password<br><input type='text' id='pasw' maxlength='16' ><br /><br />");
  espClient.print ("<button type='button' id='send' onclick='sendData()'>Save</button></div>");
  espClient.print ("<div id='w'></div><div id='footer'>Please be aware you may have to reset the ESP32 to return to control page</div></div>");
  espClient.print ("<script type='text/javascript'>");
  espClient.println ("function sendData() {");
  espClient.println ("  var ssid = document.getElementById('ssid').value;");
  espClient.println ("  var pw =   document.getElementById('pasw').value;");
  espClient.println ("  if (ssid == 'none'){ alert('Choose a WiFi Station'); return false; }");
  espClient.println ("  if (pw.length<8 ){ alert('Enter a password minimal 8 characters'); return false;  }");
  espClient.println ("  var xhttp = new XMLHttpRequest();");  // we can't use jquery so old style
  espClient.println ("  xhttp.onreadystatechange = function() {");
  espClient.println ("      if (this.readyState == 4 && this.status == 200) {");
  espClient.println ("          document.getElementById('w').innerHTML =");
  espClient.println ("          this.responseText;");
  espClient.println ("      }");
  espClient.println ("  };");
  espClient.println ("  xhttp.open('GET', '/api/"+ (String)mwSk +"?ssid=' + encodeURIComponent(ssid) + '&pw=' + encodeURIComponent(pw) + '/', true);");
  espClient.println ("  xhttp.send();");
  espClient.println ("}");
  espClient.print ("</script>");
  espClient.print ("</body></html>");
  espClient.stop();
}
void interfaceTimers(WiFiClient espClient){
  // Build the timers interface
  espClient.print ("<!DOCTYPE html><html><head><meta name='viewport' content='initial-scale=1.0'><meta charset='utf-8'>");
  espClient.print ("<style>#map {height: 100%;} html, body {height: 100%; margin: 25px; padding: 10px;font-family: Sans-Serif;}");
  espClient.print ("#container{max-width:600px;} #footer{position: absolute;bottom: 0; font-size:8px;} tr th, .tac{text-align:center;}");
  espClient.print ("td {padding:0px 2px;} tr th, tr td:nth-child(odd) { background: #cfe2f3; } input {width:50px; }</style>");
  espClient.print ("<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css'>");
  espClient.print ("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/spectrum/1.8.0/spectrum.min.css'>");
  espClient.print ("<script defer src='https://use.fontawesome.com/releases/v5.0.9/js/all.js'></script>");
  espClient.print ("</head>");
  espClient.print ("<body><div id='container'><h1>RGBw Timers</h1>");
  espClient.println("<div><table>");
  espClient.println("<tr><th colspan='2'></th><th colspan='4'>Colour</th><th></th><th></th><th></tr>");
  espClient.println("<tr><th>Start-time</th><th>End-time</th><th>Red</th><th>Green</th><th>Blue</th><th>White</th><th>Active</th><th>Save</th><th></th></tr>");
  
  for (int n = 0; n < maxTimers; n++){
     interfaceTimerRow(espClient,n);
  }
    
  espClient.println("<tr><th colspan='9'<div id='w' class='alert alert-info' role='alert'>Set timer details and save.</div></th></tr>");
  espClient.println("</table>");
  espClient.print ("");
  espClient.print ("<div id='footer' ><button type='button' id='main' class='btn btn-outline-secondary btn-sm'><i class='fas fa-sliders-h'></i>&nbsp;Colour control</button>");
  espClient.print ("<span>&nbsp;Version: " + version + "</span></div></div>");
  espClient.print ("<script src='https://code.jquery.com/jquery-3.2.1.min.js'></script><script src='https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js' ></script><script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js'></script>");
  espClient.print ("<script src='https://cdnjs.cloudflare.com/ajax/libs/spectrum/1.8.0/spectrum.min.js'></script>");
  espClient.print ("<script>");
  espClient.println ("$(document).ready(function($) {");  // jquery
  espClient.println ("  $('#main').click(function(){");
  espClient.println ("      window.location.href ='http://"+ipStr+"/';");
  espClient.println ("  });");
  espClient.println ("  $('.mc').change(function(){");
  espClient.println ("     var ck = parseInt($(this).attr('max'));");
  espClient.println ("     if (parseInt($(this).val())<0){$(this).val(0);} ");
  espClient.println ("     if (parseInt($(this).val())>ck){$(this).val(ck);} ");
  espClient.println ("  });");
  espClient.println ("  $('.store').click(function(){");
  espClient.println ("     var per= $(this).attr('id').split('-');"); // id of btn
  espClient.println ("     var startmin=parseInt($('#starthrs-'+per[1]).val()) * 60 + parseInt($('#startmin-'+per[1]).val());");
  espClient.println ("     var endmin=parseInt($('#endhrs-'+per[1]).val()) * 60 + parseInt($('#endmin-'+per[1]).val());");
  espClient.println ("     if (endmin == startmin) { alert ('Start time must be different from end time.'); return false; }    ");
  espClient.println ("     $('#w').html('Please wait until action has finished.');");
  espClient.println ("     $.get({url:'/api/command/timedat/',"); // send the request using AJAX
  espClient.println ("       dataType:'json',");
  espClient.println ("       data    :{id:per[1],stMin:startmin, endMin:endmin,act:($('#active-'+per[1]+':checked').val()=='on'?1:0),");
  espClient.println ("                 red:$('#red-'+per[1]).val(),green:$('#green-'+per[1]).val(),blue:$('#blue-'+per[1]).val(),white:$('#white-'+per[1]).val()},");
  espClient.println ("        success:function(data){");
  espClient.println ("           $('#w').html(data.message);");
  espClient.println ("        }");
  espClient.println ("    })");
  espClient.println ("");
  espClient.println ("  });");
  espClient.println ("});"); // end jquery
  espClient.print ("</script></body></html>");
  espClient.stop(); 
}
void interfaceTimerRow(WiFiClient espClient,int n){
  // Set the rows for the timers
  int totMins = runTimes[n].startTime;
  int rdHr = 0;
  int rdMin = 0;
  String Check = (runTimes[n].active == 0?"":"checked");
  String pos = (String)n; 
  // calculate the hours and minutes  
  if (totMins > 0){ rdHr = totMins/60; rdMin = totMins - (rdHr*60); }
  espClient.print ("<tr><td><input type='number' class='mc' id='starthrs-"+pos+"' min='0' max='23' value='"+(String)rdHr+"'><b>:</b>");
  espClient.println("<input type='number' class='mc' id='startmin-"+pos+"' min='0' max='59' value='"+(String)rdMin+"'></td>");
  totMins = runTimes[n].endTime;
  rdHr=0;  rdMin=0;
  if (totMins > 0){ rdHr = totMins/60; rdMin = totMins - (rdHr*60); }
  espClient.print ("<td><input type='number' class='mc' id='endhrs-"+pos+"' min='0' max='23' value='"+(String)rdHr+"'><b>:</b>");
  espClient.println ("<input type='number' class='mc' id='endmin-"+pos+"' min='0' max='59' value='"+(String)rdMin+"'></td>");
  espClient.println("<td><input type='number' class='mc' id='red-"+pos+"' min='0' max='255' value='"+(String)runTimes[n].red+"'></td>");
  espClient.println("<td><input type='number' class='mc' id='green-"+pos+"' min='0' max='255' value='"+(String)runTimes[n].green+"'></td>");
  espClient.println("<td><input type='number' class='mc' id='blue-"+pos+"' min='0' max='255' value='"+(String)runTimes[n].blue+"'></td>");
  espClient.println("<td><input type='number' class='mc' id='white-"+pos+"' min='0' max='255' value='"+(String)runTimes[n].white+"'></td>");
  espClient.println("<td class='tac'><input type='checkbox' id='active-"+pos+"' " + Check+"></td>");
  espClient.println("<td><button type='button' id='save-"+pos+"' class='store btn btn-outline-secondary btn-sm'><i class='far fa-save'></i><td></tr>");
}
void initLEDS(){
  // set the channels and the frequency for the leds
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
  // send the initialisation data to serial monitor
  Serial.println();
  Serial.println("Serial started");
  if (setupMode == true){
    Serial.println("AP started");
    Serial.print("IP address:\t");
    Serial.println(WiFi.softAPIP());
  } else {
    // Show WiFi is connected
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());
    Serial.println("Enter this address in your Internet browser.");
  }
}
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax) {
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
           EEPROM.get(EEred,currColours.red);
           EEPROM.get(EEgreen,currColours.green);
           EEPROM.get(EEblue,currColours.blue);
           EEPROM.get(EEwhite,currColours.white);
           ledcAnalogWrite(LEDC_CHANNEL_0_R, currColours.red);
           ledcAnalogWrite(LEDC_CHANNEL_1_G, currColours.green);
           ledcAnalogWrite(LEDC_CHANNEL_2_B, currColours.blue);
           ledcAnalogWrite(LEDC_CHANNEL_3_W, currColours.white);
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
            colourObj tmC = fillColourObj(Red,Green,Blue,White);
            setRGBColor(tmC,true);
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
              colourObj tmC = fillColourObj(Red,Green,Blue,White);
              setRGBColor(tmC,true);
              respMsg = "Colours set to Red: "+(String)Red+" Green: "+(String)Green+" Blue: "+(String)Blue+" White: "+(String)White;
            } else {
              errVal = true;
              respMsg = "Invalid length HEX code possible FFEEAA or FFEEAABB";
            }
           
        } else if (req.indexOf("/" + (String)mwSk) != -1) {
          String sd = recvVar(req,"/" + (String)mwSk,"ssid");
          String pw = recvVar(req,"/" + (String)mwSk,"pw");
          sd = urldecode(sd);
          pw = urldecode(pw); 
          if ( strlen(sd.c_str()) < 3 || strlen(pw.c_str()) < 8) {
              errVal = true;
              respMsg = "No correct data sent";
          } else {
             Serial.print(sd +  "   " +pw);
             write_wifi_toEEPROM(EEssid, sd, pw);
             respMsg = "SSID and Password are saved ESP will restart";
             delay(5000);
             ESP.restart();
          } 
        } else if (req.indexOf("/command/wifi") != -1) {
          delay(2000);
          setupMode = true; 
        } else if  (req.indexOf("/settimers") != -1) {
          Serial.print("set timers ");
          setupTimers = true;
        } else if (req.indexOf("/command/timedat") != -1) {
          int id = recvVar(req,"/command/timedat","id").toInt();
          int act = recvVar(req,"/command/timedat","act").toInt();
          int red = recvVar(req,"/command/timedat","red").toInt();
          int green = recvVar(req,"/command/timedat","green").toInt();
          int blue = recvVar(req,"/command/timedat","blue").toInt();
          int white = recvVar(req,"/command/timedat","white").toInt();
          int stmin = recvVar(req,"/command/timedat","stMin").toInt();
          int endmin = recvVar(req,"/command/timedat","endMin").toInt();
          ledTimer timedat = { id, stmin, endmin, red, green, blue, white, 0, act };
          write_timer_toEEPROM(timedat);
          respMsg = "The timer has been saved. Timer id is:" + (String)id;
        }
}
void publishToMQTT(){
  if (useMQTT == false){ return ;}
  //check if connected
  if (!mqttClient.connected()) {
    reconnect_mqtt();
  }
  // built the json object beware the limit is 128 Characters (pubSubClient)
  JsonObject& JSONencoder = jsonBuffer.createObject();
  JSONencoder["device"] = "ESP32 " + version;
  JSONencoder["sType"] = "RGBw";
  JSONencoder["time"] = formTime;
  JsonObject& colours = JSONencoder.createNestedObject("colours");
  colours["r"]=currColours.red;
  colours["g"]=currColours.green;
  colours["b"]=currColours.blue;
  colours["w"]=currColours.white;
  // made the buffer big if PubSubClient has update it might work for larger
  char JSONmessageBuffer[512];
  JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  if (mqttClient.connected()) {
    if (mqttClient.publish(mqttTopicPublish, JSONmessageBuffer) == true) {
      Serial.println("Success sending message by MQTT");
    } else {
      Serial.println("Error sending message by MQTT");
    }
  } else {
    Serial.println("MQTT not connected");
  }
}
void reconnect_mqtt() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    //Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    if (mqttClient.connect("ESP32Client", my_MQTT_USER, my_MQTT_PW)) {
      mqttClient.setCallback(callback);
      mqttClient.subscribe(mqttTopicSubscribe);
      //Serial.println("MQTT reconnected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void resetTimer(){
  // to prevent ESP from being unresponsive restart once a day at 01:53
  timeClient.update();
  formTime = timeClient.getFormattedTime();
  if (timeClient.getHours()==1 &&
      timeClient.getMinutes() % 53 == 0 && 
      timeClient.getSeconds() < 2 ){
      ESP.restart();
  }  
}
String recvVar(String req, String base, String nm){
   // find the value from a variable in a (url)request
   int val_base = req.indexOf(base);  // start looking after this part
   int val_id = req.indexOf(nm,val_base); // after the base find the value name - nm
   int val_eq = req.indexOf("=",val_id);  // a value name is closed by an =
   int val_end = req.indexOf('&',val_eq); // the value is close by an & if there are more
   if (val_end == -1){
      val_end = req.indexOf("/",val_eq);  // or by / if it is the last value
   }
   if (val_end == -1){
      val_end = req.indexOf(" ",val_eq);  // for the lazy ones that forget to close with /
   }
   String dat = req.substring(val_eq+1,val_end); // the value is between the = and  & / ' '
   if (  dat == ""){ dat ="0"; } // if it was  empty fill with 0so we can toInt() it.
   //Serial.print (nm + ": ");
   //Serial.println(dat);
   return dat;
}
void scanNetworks(){
  Serial.println("scan start");
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    //Serial.println("scan done");
    if (n == 0) {
    //    Serial.println("no networks found");
    } else {
        //Serial.println(" networks found");
        cmdSSID = "<select id='ssid'><option value='none' checked> - Select the station - </option>";
        for (int i = 0; i < n; ++i) {
            // add the details to a html select
            delay(10);
            cmdSSID += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + (String)WiFi.RSSI(i)+") "+ ((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?"":"*") + "</option>";
        }
        cmdSSID +="</select>";
    }
    
}
void setOneColour(uint32_t V, String Clr ){
    // set var for default
    int Pin = LED_PIN_W;
    int currTemp = currColours.white;
    int Channel = LEDC_CHANNEL_3_W;
    // set the proper pin and current colour
    if ( Clr == "red"){
       Pin = LED_PIN_R; 
       currTemp = currColours.red;
       Channel = LEDC_CHANNEL_0_R; 
    } else if (Clr == "green"){
       Pin = LED_PIN_G; 
       currTemp = currColours.green;
       Channel = LEDC_CHANNEL_1_G; 
    } else if (Clr == "blue"){
       Pin = LED_PIN_B; 
       currTemp = currColours.blue; 
       Channel = LEDC_CHANNEL_2_B;
    } 
    // Set the proper channel and frequency
    ledcSetup(Channel, LEDC_BASE_FREQ ,LEDC_TIMER_13_BIT);
    ledcAttachPin(Pin, Channel);
    for (int doUp = 0; doUp <= 255; doUp++) {
      // Increase! Cycle up loop and check what is current level
      if (doUp > currTemp && currTemp < V && doUp <= V){   
        ledcAnalogWrite(Channel, doUp);
      }
      int down = 255-doUp;
      if (down < currTemp  && currTemp > V && down >= V ) {  
        ledcAnalogWrite(Channel, down);
      }
      delay(delayMills);
    }
    //update the current colour and write to eeprom
    if (Clr == "red"){
       currColours.red = V; 
       writeData(EEred, V);
    } else if (Clr == "green"){
       currColours.green = V; 
       writeData(EEgreen, V);
    } else if (Clr == "blue"){
       currColours.blue = V; 
       writeData(EEblue, V);
    } else if (Clr == "white"){
       currColours.white = V; 
       writeData(EEwhite, V);
    } 
    // publish to MQTT
    publishToMQTT();
}
void setRGBColor(colourObj rC, bool toEEprom) {
  // all colours are called in each cycle to make all leds
  // increase or decrease at the same moment.
  // Getting more light first is to prevent flashes 
  for (int doUp = 0; doUp <= 255; doUp++) {
    if (doUp > currColours.red && currColours.red < rC.red && doUp <= rC.red){
      ledcAnalogWrite(LEDC_CHANNEL_0_R, doUp);
    }
    if (doUp > currColours.green && currColours.green < rC.green && doUp <= rC.green ){
      ledcAnalogWrite(LEDC_CHANNEL_1_G, doUp);
    }
    if (doUp > currColours.blue && currColours.blue < rC.blue && doUp <= rC.blue){
      ledcAnalogWrite(LEDC_CHANNEL_2_B, doUp);
    }
    if (doUp > currColours.white && currColours.white < rC.white && doUp <= rC.white){
      ledcAnalogWrite(LEDC_CHANNEL_3_W, doUp);
    }
    int down = 255-doUp;
    if (down < currColours.red  && currColours.red > rC.red && down >= rC.red ){
      ledcAnalogWrite(LEDC_CHANNEL_0_R, down);
    }
    if (down < currColours.green  && currColours.green > rC.green && down >= rC.green ){
      ledcAnalogWrite(LEDC_CHANNEL_1_G, down);
    }
    if (down < currColours.blue && currColours.blue > rC.blue && down >= rC.blue  ){
      ledcAnalogWrite(LEDC_CHANNEL_2_B, down);
    }
    if (down < currColours.white && currColours.white > rC.white && down >= rC.white  ){
      ledcAnalogWrite(LEDC_CHANNEL_3_W, down);
    }
    // the delay can't be too large it would stop the loop
    // 10 is about 2.5 second (255 * 10) set in vars.h
    delay (delayMills);
  }
  //Serial.println("Red: "+(String)R+" Green: "+(String)G+" Blue: "+(String)B+" White: "+(String)W);
  // store the last set colours and store in eeprom
  currColours = rC;

  if (toEEprom==true){
    writeData(EEred, rC.red);
    writeData(EEgreen, rC.green);
    writeData(EEblue, rC.blue);
    writeData(EEwhite, rC.white);
  }
  // publish to MQTT
  publishToMQTT();
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
      Serial.printf("Uploading: %u%%\r\n", (progress / (total / 100)));
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
    // switch all off really fast
    ledcAnalogWrite(LEDC_CHANNEL_0_R, 0);
    ledcAnalogWrite(LEDC_CHANNEL_1_G, 0);
    ledcAnalogWrite(LEDC_CHANNEL_2_B, 0);
    ledcAnalogWrite(LEDC_CHANNEL_3_W, 0);
    // restore to latest setting after restart
    // retrieve data from EEprom if available
    EEPROM.get(EEred,currColours.red);
    EEPROM.get(EEgreen,currColours.green);
    EEPROM.get(EEblue,currColours.blue);
    EEPROM.get(EEwhite,currColours.white);
    // switch all on really fast
    ledcAnalogWrite(LEDC_CHANNEL_0_R, currColours.red);
    ledcAnalogWrite(LEDC_CHANNEL_1_G, currColours.green);
    ledcAnalogWrite(LEDC_CHANNEL_2_B, currColours.blue);
    ledcAnalogWrite(LEDC_CHANNEL_3_W, currColours.white);
    Serial.println("First colours set!");
    justOnce = true;
  }
}
bool testRecvVal(int rVal){
   // the colour value must be between 0 and 255 both included
   if ( rVal >= 0 && rVal <= 255 ){
      return true;
   } else {
      //will show the error in response JSON
      errVal = true;
      respMsg = "colour value out of range";
      return false;
   }
}
String urldecode(String str){
    
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str.charAt(i);
      if ( c == '+'){
        encodedString+=' ';  
      }else if (c == '%') {
        i++;
        code0=str.charAt(i);
        i++;
        code1=str.charAt(i);
        c = (h2int(code0) << 4) | h2int(code1);
        encodedString+=c;
      } else{
        
        encodedString+=c;  
      }
      
      yield();
    }
    
   return encodedString;
}
String urlencode(String str){
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        encodedString+=code2;
      }
      yield();
    }
    return encodedString;
    
}
void writeData(uint8_t addr, uint32_t datInt){
    int testVar = 0;
    // to conserve flash memory only write when differs
    // only used for colours and EEset
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
void write_timer_toEEPROM(ledTimer timedat){
  // EEprom address for writing
  uint8_t startAddr = EEtimer5;
  if (timedat.id==0){
    startAddr = EEtimer1;
  } else if (timedat.id==1){
    startAddr = EEtimer2;
  } else if (timedat.id==2){
    startAddr = EEtimer3;
  } else if (timedat.id==3){
    startAddr = EEtimer4;
  }
  EEPROM.put(startAddr, timedat);
  EEPROM.commit(); 
  Serial.println("Timer data Object is saved");
  // also update the runTime object so we don't have to reload all timers 
  runTimes[timedat.id] = timedat; 
}
ledTimer read_timer_fromEEPROM(int id){
  // Read addresses for timers
  uint8_t startAddr = EEtimer5;  
  if (id==0){
    startAddr = EEtimer1;
  } else if (id==1){
    startAddr = EEtimer2;
  } else if (id==2){
    startAddr = EEtimer3;
  } else if (id==3){
    startAddr = EEtimer4;
  }
  // set the Object
  ledTimer readEE;
  EEPROM.get(startAddr, readEE);
  Serial.println("Read timer object with id: " + (String) id +" from EEPROM");
  return readEE;   
}
void write_wifi_toEEPROM(uint8_t startAddr, String strSSID, String strPW){
  // Set Object
  wifiConn vars;
  // enter the two parameters
  strSSID.toCharArray(vars.eSsid, sizeof(vars.eSsid));
  strPW.toCharArray(vars.ePasw, sizeof(vars.ePasw));
  // Put and commit
  EEPROM.put(startAddr, vars);
  EEPROM.commit();  
}
wifiConn read_wifi_fromEEPROM(uint8_t startAddr){
  wifiConn readEE;        //Variable to store custom object read from EEPROM.
  EEPROM.get(startAddr, readEE);
  Serial.print("Read wifi connection object from EEPROM will connect to ");
  Serial.println(readEE.eSsid);
  return readEE;    
}