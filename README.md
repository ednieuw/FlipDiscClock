# FlipDiscClock
Nano ESP32 code for FlipDisc clock

The software of this Flipo clock is based on the Arduino Nano ESP32 word clock and the control and functionality is similar.<br>
See here: https://github.com/ednieuw/Arduino-ESP32-Nano-Wordclock#control-of-the-clock

How to buy the clock can be found here: https://flipo.io/ <br>
and here: https://github.com/marcinsaj

To use the software:
Easy:
```
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

![image](https://github.com/user-attachments/assets/f076506d-4b0f-4e24-8999-a8cfd5927b91)

