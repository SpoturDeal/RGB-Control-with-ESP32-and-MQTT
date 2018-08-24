# RGB(w)-Control-with-ESP32-and-MQTT
An RGB(W) control on ESP32 with webinterface and MQTT status replies.

This RGB control can be used with different domotica systems.
The last set colours is stored in EEprom after a start up, it will return to last colour

## Parts

## Materials to use

Board: DOIT ESP32 DEVKIT V1, 80Mhz, 4MB(32Mhz),921600 None op COM3 <a href="https://www.banggood.com/ESP32-Development-Board-WiFiBluetooth-Ultra-Low-Power-Consumption-Dual-Cores-ESP-32-ESP-32S-Board-p-1109512.html?p=VQ141018240205201801">Available at Banggood</a>

PowerSupply(1):  5 Volt <a href="https://www.banggood.com/3Pcs-DC-DC-4_5-40V-Step-Down-LED-Voltmeter-USB-Voltage-Converter-Buck-Module-5V2A-p-1178249.html?p=VQ141018240205201801">Available at Banggood</a>
 
PowerSupply(2): 12 Volt <a href="https://www.banggood.com/AC-100-240V-to-DC-12V-5A-60W-Power-Supply-Adapter-For-LED-Strip-Light-p-994870.html?p=VQ141018240205201801">Available at Banggood</a>

## Schematics

Coming soon

## Updates

|Date|Description|
|--|--|
|24th August 2018|Added support for setup by direct AP connection to ESP32 and connect to MQTT|
|21th August 2018|Added Range sliders in user interface|

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
{"error":false,"status":"OK","time":"","message":"LED Blue set to: 121","eeprom":"Data has been updated"}



## Screenshots
#### RGB(W) Webinterface 
![Webserver](/rgbinterface.png?raw=true "RGB webinterface")

Coming soon before 30th August 2018
