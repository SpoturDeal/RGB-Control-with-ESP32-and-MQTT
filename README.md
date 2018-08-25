# RGB(w)-Control-with-ESP32-and-MQTT
An RGB(W) control on ESP32 with webinterface and MQTT status replies.

This RGB control can be used with different domotica systems.
The last set colours is stored in EEprom after a start up, it will return to last colour.

With the few parts you have full IoT control over your RGB devices for less then EUR 15.00

Once you have uploaded the sketch just find "Esp32_SetUp" in your networks and connect. Then open your browser and enter 192.168.4.1 here you select the Wireless Network you want to use and the password. After clicking send and the confirmation the ESP will restart.

On a mobile device you can use ***fing*** on <a href="https://play.google.com/store/apps/details?id=com.overlook.android.fing&hl=nl">Google Play</a> or <a href="https://itunes.apple.com/nl/app/fing-netwerk-scanner/id430921107?mt=8">Apple iTunes</a> to find the IP address given to the ESP32.

## Parts

## Materials to use

Board: DOIT ESP32 DEVKIT V1, 80Mhz, 4MB(32Mhz),921600 None op COM3 <a href="https://www.banggood.com/ESP32-Development-Board-WiFiBluetooth-Ultra-Low-Power-Consumption-Dual-Cores-ESP-32-ESP-32S-Board-p-1109512.html?p=VQ141018240205201801">Available at Banggood</a>

PowerSupply(1):  5 Volt <a href="https://www.banggood.com/3Pcs-DC-DC-4_5-40V-Step-Down-LED-Voltmeter-USB-Voltage-Converter-Buck-Module-5V2A-p-1178249.html?p=VQ141018240205201801">Available at Banggood</a>
 
PowerSupply(2): 12 Volt <a href="https://www.banggood.com/AC-100-240V-to-DC-12V-5A-60W-Power-Supply-Adapter-For-LED-Strip-Light-p-994870.html?p=VQ141018240205201801">Available at Banggood</a>

## Schematics

Coming soon

## Updates

|Date|Version|Description|
|--|--|--|
|25th August 2018|0.9.3|First fully working version|
|24th August 2018|0.8.2|Added support for setup by direct AP connection to ESP32 and connect to MQTT|
|21th August 2018|0.8.1|Added Range sliders in user interface|

## Usage in browser
always use closing /

|request|extra information|
|--|--|
|http:{ipaddress}/command/on/|To latest colour|
|http:{ipaddress}/command/off/||
|http:{ipaddress}/command/color=255,128,64,32/|For decimal data (also only RGB 255,128,64)|
|http:{ipaddress}/command/hex=FFEEDDCC/|For hex data (also only RGB HH0A11)|
|http:{ipaddress}/command/red=255/|For colours red, green, blue, white  (0 to 255)|

## Usage with domotica
always use closing /

|request|extra information|
|--|--|
|http:{ipaddress}/api/command/on/|To latest colour|
|http:{ipaddress}/api/command/off/||
|http:{ipaddress}/api/command/color=255,128,64,32/|For decimal data (also only RGB 255,128,64)|
|http:{ipaddress}/api/command/hex=FFEEDDCC/|For hex data (also only RGB HH0A11)|
|http:{ipaddress}/api/command/red=255/|For colours red, green, blue, white  (0 to 255)|

## api reply in json
```
/*
   {"error":false,
    "status":"OK",
    "message":"LED Blue set to: 129",
    "colours":{"red":0,
               "green":0,
               "blue":129,
               "white":0},
    "updated":{"time":"14:11:45",
               "eeprom":"Data has been updated",
               "version":"0.9.3"}
   }
*/

```

## reply to MQTT
```
/*
esp/out {"device":"ESP32",
         "sensorType":"RGB Control",
         "time":"14:11:45",
         "colours":{"red":0,
                    "green":0,
                    "blue":129,
                    "white":0}
        }
*/

```

## Screenshots
#### RGB(W) Webinterface 
![User interface](/RGBW_control.png?raw=true "RGB webinterface") If you change a value with one the slider also the color in the colour picker will be updated and vice versa. The **change WiFi** button brings the ESP in Acces Point mode. If you don't enter a password and send 
you need to restart the ESP to exit AP mode.


![SetUp interface](/RGBW_SetUp.png?raw=true "RGB webinterface") If you don't enter a password and send you need to restart the ESP to exit AP mode.
If your password is not stored it will go back in AP mode until a valid password/ssid combination is entered.



Coming soon before 30th August 2018
