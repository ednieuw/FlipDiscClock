# Arduino Nano ESP32 code for FlipDisc clock.

![image](https://github.com/user-attachments/assets/f076506d-4b0f-4e24-8999-a8cfd5927b91)

The Flip disk clock with this software connects to the internet to receive time from it. <br>
- Time can be set to the time zone the clocks operates in. Daylight saving is taking care for.<br>
- If no WIFI is available the clock can be set to operate with a precise internal RTC-clock. No daylight saving then.<br>
- Flipo display can be turned off and silenced between two times or be turned on and off directly.<br>
- The separator 1x3 flip disc can be the standard two dots, tick per second or show 15, 30, 45 seconds.<br>
- Settings are stored in memory of the microcontroller.<bt>
- A menu to enter settings or commands can be accessed via thr serial port, bluetooth with phone tablet of pc, or a html page.
- Flipdisc display changing speed in menu.
- Display if the clock in using the RTC or NTP time at startup.

The software of this Flip disc clock is based on the Arduino Nano ESP32 word clock and the control and functionality is similar.<br>
While this page is still under construction:<br>
See here: https://github.com/ednieuw/Arduino-ESP32-Nano-Wordclock#control-of-the-clock

To do:
- buttons to set the RTC time if no WIFI/NTP available and switching between RTC and NTP time 
- write a manual

Where to obtain the clock can be found here: https://flipo.io/ <br>
and here: https://github.com/marcinsaj

To enter the WIFI router's SSID and password the clock must be connected to a PC with with a serial terminal program or with a BLE serial terminal app that run on IOS or Android.

Use the IOS app for iPhone or iPad: [BLE Serial Pro](https://apps.apple.com/nl/app/ble-serial-pro/id1632245655?l=en). <br />
Or the free less sophisticated app: [BLE serial nRF](https://apps.apple.com/nl/app/bleserial-nrf/id1632235163?l).<br>
Tip: Turn on Fast BLE with option Z in the menu for a faster transmission. 

For Android use: [Serial Bluetooth terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal). <br />
Turn off (default) Fast BLE in the menu. 

To use the software:
```
Easy:
A Load Arduino OTA program from the Examples in the Arduino IDE in the Arduino Nano ESP32
B Open the web page of the uploaded program in the Arduino Nano ESP32
C Load the bin file from this repository
D Open the serial monitor at 115200 in the Arduino IDE
E Send and I in the Serial monitor and a menu will be printed
F Send the charactes A followed with your WIFI SSID name
G Send the character B followed with the password from this SSID
H Send the character @ to reset the Arduino Nano ESP32 inside the clock
I Power the clock and wait ....

Less Easy.
Compile and uoload the program to the Nano ESP32 
Continue with D Open the serial monitor at 115200 in the Arduino IDE
```



