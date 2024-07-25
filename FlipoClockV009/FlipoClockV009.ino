/* 
How to compile: 

Check if ELEGANTOTA_USE_ASYNC_WEBSERVER 1 in ElegantOTA.h
// Locate the ELEGANTOTA_USE_ASYNC_WEBSERVER macro in the ElegantOTA.h file, and set it to 1:
// #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
Check if ESPAsyncWebServer ffrom 
Install Arduino Nano ESP32 boards
Board: Arduino Nano ESP32
Partition Scheme: With FAT
Pin Numbering: By GPIO number (legacy)

 Author .    : Ed Nieuwenhuys
 Changes V001: Derived from ESP32Arduino_WordClockV034
 Changes V002: 
 Changes V003: No errors during compilation. Basic program
 Changes V004: Flipo BLE and WIFI working 
 Changes V005: Added seconds and 15-seconds tick
 Changes V006: HTML page, Flipos On/off, Demo mode (in seconds)
 Changes V007: Get time from RX8025T. Using Wire.h for DS3231 and RX8025T. 
 Changes V008: Time is working fifty fifty. Removed RTCLib
 Changes V009: Clean up time codes. Working version
*/
// =============================================================================================================================

//--------------------------------------------
// ESP32 Includes defines and initialisations
//--------------------------------------------

#include <Preferences.h>
#include <NimBLEDevice.h>      // For BLE communication  https://github.com/h2zero/NimBLE-Arduino
#include <ESPNtpClient.h>      // https://github.com/gmag11/ESPNtpClient
#include <WiFi.h>              // Used for web page 
#include <AsyncTCP.h>          // Used for webpage  https://github.com/dvarrel/AsyncTCP                    old https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWebServer.h> // Used for webpage  https://github.com/mathieucarbou/ESPAsyncWebServer     Old one with  https://github.com/me-no-dev/ESPAsyncWebServer
#include <ElegantOTA.h>        // If a large bunch of compile errors? See here :https://docs.elegantota.pro/async-mode
                      // Locate the ELEGANTOTA_USE_ASYNC_WEBSERVER macro in the ElegantOTA.h file, and set it to 1:
                      // #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <FlipDisc.h>          // https://github.com/marcinsaj/FlipDisc
#include <Wire.h>              // Ter zijner tijd Wire functies gaan gebruiken. Staan al klaar in de code 

//------------------------------------------------------------------------------
// PIN Assigments for Arduino Nano ESP32
//------------------------------------------------------------------------------ 
 /* Attention: do not change! Changing these settings may physical damage the flip-disc displays.
Pin declaration for a dedicated controller */
#define EN_PIN A7  // Start & End SPI transfer data
#define CH_PIN A2  // Charging PSPS module - turn ON/OFF
#define PL_PIN A3  // Release the current pulse - turn ON/OFF 

/* Buttons - counting from the top */
#define B1_PIN 10  // Increment button
#define B2_PIN 9   // Settings button
#define B3_PIN 8   // Decrement button

/* RTC */
#define RTC_PIN A1 // Interrupt input


enum DigitalPinAssignments {      // Digital hardware constants ATMEGA 328 ----
 SERRX        = D0,               // D1 Connects to Bluetooth TX
 SERTX        = D1,               // D0 Connects to Bluetooth RX
 encoderPinA  = D3,               // D3 right (labeled DT on decoder)on interrupt  pin
 clearButton  = D4,               // D4 switch (labeled SW on decoder)
 LED_PIN      = D5,               // D5 Pin to control colour SK6812/WS2812 LEDs
 encoderPinB  = D8,               // Flipo B3_PIN 8   // Decrement button    //D8 left (labeled CLK on decoder)no interrupt pin  
 PCB_LED_D09  = D9,               // Flipo B2_PIN 9   // Settings button     //D9
 PCB_LED_D10  = D10,              // Flipo B3_PIN 8   // Decrement button    //D10
 secondsPin   = 48,               // D13  GPIO48 (#ifdef LED_BUILTIN  #undef LED_BUILTIN #define LED_BUILTIN 48 #endif)
 };
 
enum AnaloguePinAssignments {     // Analogue hardware constants ----
 EmptyA0      = A0,               // Empty
 EmptyA1      = A1,               // Flipo RTC_PIN
 PhotoCellPin = A2,               // CH_PIN A2  // Charging PSPS module - turn ON/OFF   //  LDR pin
 OneWirePin   = A3,               // PL_PIN A3  // Release the current pulse - turn ON/OFF  /// OneWirePin
 SDA_pin      = A4,               // SDA pin
 SCL_pin      = A5,               // SCL pin
 EmptyA6     =  A6,               // Empty
 EmptyA7     =  A7};              // EN_PIN A7  // Start & End SPI transfer data    //Empty


//--------------------------------------------                                                //
// FLIPO 
//--------------------------------------------

int flip_disc_delay_time = 10;                                                                //  Set the delay effect between flip discs. Recommended delay range: 0 - 100ms, max 255ms
bool SecondsTicks        = false;
bool FifteenSecondsTick  = false;

volatile bool setButtonPressedStatus           = false;                                       // The flag stores the status of pressing the middle button
volatile bool setButtonPressedLongStatus       = false;                                       // The flag stores the status of long pressing the middle button
volatile unsigned long setButtonPressStartTime = 0;                                           // Variable for storing the time the middle button was pressed
const unsigned long longPressTime              = 2000;                                        // Long press time in milliseconds

/* The variable stores the current level of time settings 
level 0 - normal operation
level 1 - settings of tenths of hours
level 2 - setting of hour units
level 3 - setting of tenths of minutes
level 4 - setting of minute units */
//volatile uint8_t settingsLevel = 0; 

/* The flag to store status of RTC interrupt */
//volatile bool rtcInterruptStatus = false;
/* A flag that stores the time display status, if the flag is set, the current time will be displayed */
//volatile bool timeDisplayStatus = false;
/* The flag to store status of the settings status, if the flag is set, the time setting option is active */
//volatile bool timeSetStatus = false;

/* An array to store individual digits for display */
int digit[4] = {0, 0, 0, 0};

//--------------------------------------------
// DS3231 CLOCK MODULE
//--------------------------------------------
#define DS3231_I2C_ADDRESS          0x32            //0x68  = DS3231 address
#define DS3231_TEMPERATURE_MSB      0x11
#define DS3231_TEMPERATURE_LSB      0x12
byte DS3231Installed = 0;                                        // True if the DS3231 is detected

//--------------------------------------------
// RX8025T CLOCK MODULE
//--------------------------------------------
#define RX8025T_ADDR                0x32                         // RX8025T I2C Address
#define RX8025T_SECONDS             0x00                         // RX8025T Register Addresses


//--------------------------------------------
// CLOCK initialysations
//--------------------------------------------                                 

static uint32_t msTick;                                                                       // Number of millisecond ticks since we last incremented the second counter
byte      lastminute = 0, lasthour = 0, lastday = 0, sayhour = 0;
bool      Demo                 = false;
bool      Zelftest             = false;
bool      Is                   = true;                                                        // toggle of displaying Is or Was
bool      ZegUur               = true;                                                        // Say or not say Uur in NL clock
struct    tm timeinfo;                                                                        // Storage of time used in clock for time display
struct    tm RTCtm;                                                                           // Storage of time in RTC
//--------------------------------------------                                                //
// BLE   //#include <NimBLEDevice.h>
//--------------------------------------------
BLEServer *pServer      = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected    = false;
bool oldDeviceConnected = false;
std::string ReceivedMessageBLE;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"                         // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


//----------------------------------------
// WEBSERVER
//----------------------------------------
int WIFIConnected = 0;                                                                        // Is wIFI connected?
AsyncWebServer server(80);                                                                    // For OTA Over the air uploading
#include "Webpage.h"

//--------------------------------------------                                                //
// NTP
//----------------------------------------
boolean syncEventTriggered = false;                                                           // True if a time even has been triggered
NTPEvent_t ntpEvent;                                                                          // Last triggered event

//--------------------------------------------
// SPIFFS storage
//--------------------------------------------
Preferences FLASHSTOR;
//----------------------------------------
// Common
//----------------------------------------
 uint64_t Loopcounter = 0;
#define MAXTEXT 140
char sptext[MAXTEXT];                                                                        // For common print use 
bool LEDsAreOff       = false;                                                               // If true LEDs are off except time display
byte TestLDR            = 0;                                                                 // If true LDR inf0 is printed every second in serial monitor
uint16_t MilliSecondValue  = 2000;                                                           // The duration of a second  minus 1 ms. Used in Demo mode
struct EEPROMstorage {                                                                       // Data storage in EEPROM to maintain them after power loss
  byte DisplayChoice    = 0;
  byte TurnOffLEDsAtHH  = 0;
  byte TurnOnLEDsAtHH   = 0;
  byte LanguageChoice   = 0;
  byte LightReducer     = 0;
  int  LowerBrightness  = 0;
  int  UpperBrightness  = 0;
  int  NVRAMmem[24];                                                                          // LDR readings
  byte BLEOnOff         = 1;
  byte NTPOnOff         = 1;
  byte WIFIOnOff        = 1;  
  byte StatusLEDOnOff   = 1;
  int  ReconnectWIFI    = 0;                                                                  // No of times WIFI reconnected 
  byte UseSDcard        = 0;
  byte WIFINoOfRestarts = 0;                                                                  // If 1 than resart MCU once
  byte UseDS3231        = 0;                                                                  // Use the DS3231 time module 
  byte LEDstrip         = 0;                                                                  // 0 = SK6812 LED strip. 1 = WS2812 LED strip
  byte ByteFuture3      = 0;                                                                  // For future use
  byte ByteFuture4      = 0;                                                                  // For future use
  int  IntFuture1       = 0;                                                                  // For future use
  int  IntFuture2       = 0;                                                                  // For future use
  int  IntFuture3       = 0;                                                                  // For future use
  int  IntFuture4       = 0;                                                                  // For future use   
  byte UseBLELongString = 0;                                                                  // Send strings longer than 20 bytes per message. Possible in IOS app BLEserial Pro 
  uint32_t OwnColour    = 0;                                                                  // Self defined colour for clock display
  uint32_t DimmedLetter = 0;
  uint32_t BackGround   = 0;
  char Ssid[30];                                                                              // 
  char Password[40];                                                                          // 
  char BLEbroadcastName[30];                                                                  // Name of the BLE beacon
  char Timezone[50];
  int  Checksum        = 0;
}  Mem; 
//--------------------------------------------                                                //
// Menu
//0        1         2         3         4
//1234567890123456789012345678901234567890----  
 char menu[][40] = {
 "A SSID B Password C BLE beacon name",
 "D Date (D15012021) T Time (T132145)",
 "E Timezone  (E<-02>2 or E<+01>-1)",
 "F Toggle 15 seconds tick",
 "I To print this Info menu",
 "J Toggle use RTC module",
 "K Reads/sec toggle On/Off", 
//  "L",
 "M Demo mode (sec) (M2)",
 "N Display off between Nhhhh (N2208)",
 "O Display toggle On/Off",
 "P Status LED toggle On/Off", 
//  "Q ",
 "R Reset settings @ = Reset MCU",
 "S Toggle Seconds tick",
 "W=WIFI  X=NTP& Y=BLE  Z=Fast BLE",
 "Ed Nieuwenhuys August 2024" };
 
//  -------------------------------------   End Definitions  ---------------------------------------

//--------------------------------------------
// FLIPO Init
//--------------------------------------------
void InitFlipo(void)
{
 Flip.Pin(EN_PIN, CH_PIN, PL_PIN);
 pinMode(B1_PIN, INPUT);
 pinMode(B2_PIN, INPUT);
 pinMode(B3_PIN, INPUT);
 pinMode(RTC_PIN, INPUT_PULLUP);
   /* Assigning the interrupt handler to the button responsible for time settings */
 // attachInterrupt(digitalPinToInterrupt(B2_PIN), setButtonPressedISR, RISING);
  
  /* Attention: do not change! Changing these settings may physical damage the flip-disc displays. 
  Flip.Init(...); it is the second most important function. Initialization function for a series 
  of displays. The function also prepares SPI to control displays. Correct initialization requires 
  code names of the serially connected displays:
  - D7SEG - 7-segment display
  - D3X1 - 3x1 display */
  Flip.Init(D7SEG, D7SEG, D3X1, D7SEG, D7SEG);

  /* This function allows you to display numbers and symbols
  Flip.Matrix_7Seg(data1,data2,data3,data4); */ 
  Flip.Matrix_7Seg(T,I,M,E);

  /* Function allows you to control one, two or three discs of the selected D3X1 display.
  - Flip.Display_3x1(module_number, disc1, disc2, disc3); */
  Flip.Display_3x1(1, 1,0,0);
}


//--------------------------------------------
// FLIPO DisplayTime
//--------------------------------------------
void DisplayFlipoTime(void)
{
  /* The function is used to set the delay effect between flip discs. 
  The default value without calling the function is 0. Can be called multiple times 
  anywhere in the code. Recommended delay range: 0 - 100ms, max 255ms */
  Flip.Delay(flip_disc_delay_time);
  
  /* Get the time from the RTC and save it to the tm structure */
  //RTC_RX8025T.read(tm);
   // GetTijd(0);                                                                                 // get the time for the seconds 
  /* Extract individual digits for the display */
  digit[0] = (timeinfo.tm_hour / 10) % 10;
  digit[1] = (timeinfo.tm_hour / 1) % 10;
  digit[2] = (timeinfo.tm_min / 10) % 10;
  digit[3] = (timeinfo.tm_min / 1) % 10;

  /* Display the current time */
  Flip.Matrix_7Seg(digit[0],digit[1],digit[2],digit[3]);
  
  // /* Print time to the serial monitor */
   Print_tijd();
  // The delay effect is used only when displaying time.  During time settings, the delay is 0. 
  Flip.Delay(0);
}


//--------------------------------------------                                                //
//  FLIPO 
//--------------------------------------------




//--------------------------------------------                                                //
// ARDUINO Setup
//--------------------------------------------
void setup() 
{
 Serial.begin(115200);                                                                       // Setup the serial port to 115200 baud //      
 
 pinMode(secondsPin, OUTPUT);                                                                 // turn On seconds LED-pin
 pinMode(LED_RED,   OUTPUT);
 pinMode(LED_GREEN, OUTPUT);
 pinMode(LED_BLUE,  OUTPUT);
 
 SetStatusLED(1,0,0);                                                                         // Set the status LED to red
 Wire.begin();
 int32_t Tick = millis();  
 while (!Serial)  
 {if ((millis() - Tick) >5000) break;}  Tekstprintln("Serial started");                       // Wait max 5 seconds until serial port is started   
 InitDS3231Mod();                       Tekstprintln("DS3231 RTC software started");          // Start the DS3231 RTC-module even if not installed. It can be turned it on later in the menu
 InitStorage();                         Tekstprintln("Stored settings loaded");               // Load settings from storage and check validity 
 InitFlipo();
 if(Mem.BLEOnOff) { StartBLEService();  Tekstprintln("BLE started"); }                        // Start BLE service
   Flip.Display_3x1(1, 0,0,1);
 if(Mem.WIFIOnOff){StartWIFI_NTP();     Tekstprintln("WIFI started");   }                     // Start WIFI and optional NTP if Mem.WIFIOnOff = 1 
 SWversion();                                                                                 // Print the menu + version 
 GetTijd(1);                                                                                  // Get the time and print it
 Displaytime();                                                                               // Print the tekst time in the display 
 Tekstprintln(""); 
 Flip.Display_3x1(1, 1,1,0);
 msTick = millis();                                                                           // start the seconds loop counter
}
//--------------------------------------------                                                //
// ARDUINO Loop
//--------------------------------------------
void loop() 
{
 Loopcounter++;
 if (Demo)         Demomode();
 else              EverySecondCheck();                                                        // Let the second led tick and run the clock program
// EverySecondCheck();  
 CheckDevices();
}
//--------------------------------------------                                                //
// COMMON Check connected input devices
//--------------------------------------------
void CheckDevices(void)
{
 CheckBLE();                                                                                  // Something with BLE to do?
 SerialCheck();                                                                               // Check serial port every second 
 ElegantOTA.loop();                                                                           // For Over The Air updates This loop block is necessary for ElegantOTA to handle reboot after OTA update.
// ButtonsCheck();                                                                            // Check if buttons pressed
}
//--------------------------------------------                                                //
// COMMON Update routine done every second
//--------------------------------------------
void EverySecondCheck(void)
{
 static bool Toggle = true;
 uint32_t msLeap = millis() - msTick;                                                         // 
 if (msLeap >999)                                                                             // Every second enter the loop
 {
  msTick = millis();
  GetTijd(0);                                                                                 // get the time for the seconds 
  Toggle = !Toggle;                                                                           // Used to turn On or Off Leds
  UpdateStatusLEDs(Toggle);
 if(TestLDR)
   {
    if (Mem.UseDS3231)  sprintf(sptext,"%5lld l/s %0.0f°C ", Loopcounter,GetDS3231Temp()); 
    else                sprintf(sptext,"%5lld l/s ", Loopcounter);   
    Tekstprint(sptext);
    Print_tijd();  
   }
  if(SecondsTicks)           Flip.Display_3x1(1, 1,1,Toggle);
  if(FifteenSecondsTick) 
   {
    if (timeinfo.tm_sec == 0) Flip.Display_3x1(1, 0,0,0);
    if (timeinfo.tm_sec ==15) Flip.Display_3x1(1, 0,0,1);
    if (timeinfo.tm_sec ==30) Flip.Display_3x1(1, 0,1,1);
    if (timeinfo.tm_sec ==45) Flip.Display_3x1(1, 1,1,1); 
   }
  if (timeinfo.tm_min != lastminute) EveryMinuteUpdate();                                     // Enter the every minute routine after one minute; 
  Loopcounter=0;
 }  
}

//--------------------------------------------                                                //
// COMMON Update routine done every minute
//-------------------------------------------- 
void EveryMinuteUpdate(void)
{   
 lastminute = timeinfo.tm_min;  
 GetTijd(0);
 Displaytime();                                                                               //Print_RTC_tijd();
 // SecondUpdate(true);   
 if(timeinfo.tm_hour != lasthour) EveryHourUpdate();
}
//--------------------------------------------                                                //
// COMMON Update routine done every hour
//--------------------------------------------
void EveryHourUpdate(void)
{
  if(Mem.WIFIOnOff)
    {
     if( WiFi.localIP()[0]==0 && strlen(Mem.Password)>4)                                      // If lost connection and a password entered
       {
        sprintf(sptext, "Disconnected from station, attempting reconnection.");
        Tekstprintln(sptext);
        WiFi.reconnect();
       }
    }
 lasthour = timeinfo.tm_hour;
 if (!Mem.StatusLEDOnOff) SetStatusLED(0,0,0);                                                // If for some reason the LEDs are ON and after a MCU restart turn them off.  
 if(timeinfo.tm_hour == Mem.TurnOffLEDsAtHH){ LEDsAreOff = true;  ClearScreen(); }            // Is it time to turn off the LEDs?
 if(timeinfo.tm_hour == Mem.TurnOnLEDsAtHH)   LEDsAreOff = false;                             // 
 if (timeinfo.tm_mday != lastday) EveryDayUpdate();  
}
//--------------------------------------------                                                //
// COMMON Update routine done every day
//--------------------------------------------
void EveryDayUpdate(void)
{
 if(timeinfo.tm_mday != lastday) 
   {
    lastday           = timeinfo.tm_mday; 
//    StoreStructInFlashMemory();                                                             // 
    }
}

//--------------------------------------------                                                //
// COMMON Update routine for the status LEDs
//-------------------------------------------- 
void UpdateStatusLEDs(bool Toggle)
{
 if(Mem.StatusLEDOnOff)   
   {
    SetStatusLED(Toggle && !WIFIConnected, Toggle && WIFIConnected, Toggle && deviceConnected); 
    digitalWrite(PCB_LED_D09,  Toggle);                                                       //
    digitalWrite(PCB_LED_D10, !Toggle);                                                       //
    digitalWrite(secondsPin,  Toggle);     
   }
   else
   {
    SetStatusLED(0,0,0); 
    digitalWrite(PCB_LED_D09, 0);                                                             //
    digitalWrite(PCB_LED_D10, 0);                                                             //
    digitalWrite(secondsPin,  0);                                                             // on Nano ESP32 Turn the LED off    
   }
}
//--------------------------------------------                                                //
// COMMON check for serial input
//--------------------------------------------
void SerialCheck(void)
{
 String SerialString; 
 while (Serial.available())
    { 
     char c = Serial.read();                                                                  // Serial.write(c);
     if (c>31 && c<128) SerialString += c;                                                    // Allow input from Space - Del
     else c = 0;                                                                              // Delete a CR
    }
 if (SerialString.length()>0) 
    {
     ReworkInputString(SerialString);                                                         // Rework ReworkInputString();
     SerialString = "";
    }
}

//--------------------------------------------                                                //
// COMMON Reset to default settings
//--------------------------------------------
void Reset(void)
{
 Mem.Checksum         = 25065;                                                                //
 Mem.DisplayChoice    = 0;                                                            // Default colour scheme 
 Mem.OwnColour        = 0;                                                                // Own designed colour.
 Mem.DimmedLetter     = 0;
 Mem.BackGround       = 0; 
 Mem.LanguageChoice   = 0;                                                                    // 0 = NL, 1 = UK, 2 = DE, 3 = FR, 4 = Wheel
 Mem.LightReducer     = 0;                                                      // Factor to dim ledintensity with. Between 0.1 and 1 in steps of 0.05
 Mem.UpperBrightness  = 0;                                                        // Upper limit of Brightness in bits ( 1 - 1023)
 Mem.LowerBrightness  = 0;                                                        // Lower limit of Brightness in bits ( 0 - 255)
 Mem.TurnOffLEDsAtHH  = 0;                                                                    // Display Off at nn hour
 Mem.TurnOnLEDsAtHH   = 0;                                                                    // Display On at nn hour Not Used
 Mem.BLEOnOff         = 1;                                                                    // default BLE On
 Mem.UseBLELongString = 0;                                                                    // Default off. works only with iPhone/iPad with BLEserial app
 Mem.NTPOnOff         = 0;                                                                    // NTP default off
 Mem.WIFIOnOff        = 0;                                                                    // WIFI default off
 Mem.ReconnectWIFI    = 0;                                                                    // Correct time if necessary in seconds
 Mem.WIFINoOfRestarts = 0;                                                                    //  
 Mem.UseSDcard        = 0;                                                                    // default off
 Mem.UseDS3231        = 0;                                                                    // Default off
 Mem.LEDstrip         = 0;    //Do not erase this setting with a reset                        // 0 = SK6812, 1=WS2812
                            
 TestLDR              = 0;                                                                    // If true LDR display is printed every second
 
 strcpy(Mem.Ssid,"");                                                                         // Default SSID
 strcpy(Mem.Password,"");                                                                     // Default password
 strcpy(Mem.BLEbroadcastName,"FlipoClock");
 strcpy(Mem.Timezone,"CET-1CEST,M3.5.0,M10.5.0/3");                                           // Central Europe, Amsterdam, Berlin etc.                                                         // WIFI On  

 Tekstprintln("**** Reset of preferences ****"); 
 StoreStructInFlashMemory();                                                                  // Update Mem struct       
 GetTijd(0);                                                                                  // Get the time and store it in the proper variables
 SWversion();                                                                                 // Display the version number of the software
 Displaytime();
}
//--------------------------------------------                                                //
// COMMON common print routines
//--------------------------------------------
void Tekstprint(char const *tekst)    { if(Serial) Serial.print(tekst);  SendMessageBLE(tekst);sptext[0]=0;   } 
void Tekstprintln(char const *tekst)  { sprintf(sptext,"%s\n",tekst); Tekstprint(sptext);  }
void TekstSprint(char const *tekst)   { printf(tekst); sptext[0]=0;}                          // printing for Debugging purposes in serial monitor 
void TekstSprintln(char const *tekst) { sprintf(sptext,"%s\n",tekst); TekstSprint(sptext); }
//--------------------------------------------                                                //
//  COMMON String upper
//--------------------------------------------
void to_upper(char* string)
{
 const char OFFSET = 'a' - 'A';
 while (*string) {(*string >= 'a' && *string <= 'z') ? *string -= OFFSET : *string;   string++;  }
}
//--------------------------------------------                                                //
// COMMON Constrain a string with integers
// The value between the first and last character in a string is returned between the low and up bounderies
//--------------------------------------------
int SConstrainInt(String s,byte first,byte last,int low,int up){return constrain(s.substring(first, last).toInt(), low, up);}
int SConstrainInt(String s,byte first,          int low,int up){return constrain(s.substring(first).toInt(), low, up);}
//--------------------------------------------                                                //
// COMMON Init and check contents of EEPROM
//--------------------------------------------
void InitStorage(void)
{
 // if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){ Tekstprintln("Card Mount Failed");   return;}
 // else Tekstprintln("SPIFFS mounted"); 

 GetStructFromFlashMemory();
 if( Mem.Checksum != 25065)
   {
    sprintf(sptext,"Checksum (25065) invalid: %d\n Resetting to default values",Mem.Checksum); 
    Tekstprintln(sptext); 
    Reset();                                                                                  // If the checksum is NOK the Settings were not set
   }
 if(strlen(Mem.Password)<5 || strlen(Mem.Ssid)<3)     Mem.WIFIOnOff = Mem.NTPOnOff = 0;       // If ssid or password invalid turn WIFI/NTP off 
 StoreStructInFlashMemory();
}
//--------------------------------------------                                                //
// COMMON Store mem.struct in FlashStorage or SD
// Preferences.h  
//--------------------------------------------
void StoreStructInFlashMemory(void)
{
  FLASHSTOR.begin("Mem",false);       //  delay(100);
  FLASHSTOR.putBytes("Mem", &Mem , sizeof(Mem) );
  FLASHSTOR.end();          
  
// Can be used as alternative
//  SPIFFS
//  File myFile = SPIFFS.open("/MemStore.txt", FILE_WRITE);
//  myFile.write((byte *)&Mem, sizeof(Mem));
//  myFile.close();
 }
//--------------------------------------------                                                //
// COMMON Get data from FlashStorage
// Preferences.h
//--------------------------------------------
void GetStructFromFlashMemory(void)
{
 FLASHSTOR.begin("Mem", false);
 FLASHSTOR.getBytes("Mem", &Mem, sizeof(Mem) );
 FLASHSTOR.end(); 

// Can be used as alternative if no SD card
//  File myFile = SPIFFS.open("/MemStore.txt");  FILE_WRITE); myFile.read((byte *)&Mem, sizeof(Mem));  myFile.close();

 sprintf(sptext,"Mem.Checksum = %d",Mem.Checksum);Tekstprintln(sptext); 
}

//--------------------------------------------                                                //
// COMMON Version info
//--------------------------------------------
void SWversion(void) 
{ 
 #define FILENAAM (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
 PrintLine(35);
 for (uint8_t i = 0; i < sizeof(menu) / sizeof(menu[0]); Tekstprintln(menu[i++]));
 PrintLine(35);
 sprintf(sptext,"Display off between: %02dh - %02dh",Mem.TurnOffLEDsAtHH, Mem.TurnOnLEDsAtHH);  Tekstprintln(sptext);
 sprintf(sptext,"SSID: %s", Mem.Ssid);                                                       Tekstprintln(sptext); 
// sprintf(sptext,"Password: %s", Mem.Password);                                             Tekstprintln(sptext);
 sprintf(sptext,"BLE name: %s", Mem.BLEbroadcastName);                                       Tekstprintln(sptext);
 sprintf(sptext,"IP-address: %d.%d.%d.%d (/update)", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );  Tekstprintln(sptext);
 sprintf(sptext,"Timezone:%s", Mem.Timezone);                                                Tekstprintln(sptext); 
 sprintf(sptext,"%s %s %s %s", Mem.WIFIOnOff?"WIFI=On":"WIFI=Off", 
                               Mem.NTPOnOff? "NTP=On":"NTP=Off",
                               Mem.BLEOnOff? "BLE=On":"BLE=Off",
                               Mem.UseBLELongString? "FastBLE=On":"FastBLE=Off" );           Tekstprintln(sptext);    
 sprintf(sptext,"Software: %s",FILENAAM);                                                    Tekstprintln(sptext);  //VERSION); 
 Print_RTC_tijd(); Tekstprintln(""); 
 PrintLine(35);
}
//--------------------------------------------                                                //
// COMMON PrintLine
//--------------------------------------------
void PrintLine(byte Lengte)
{
 for(int n=0; n<Lengte; n++) sptext[n]='_';
 sptext[Lengte] = 0;
 Tekstprintln(sptext);
}


//--------------------------------------------                                                //
//  COMMON Input from Bluetooth or Serial
//--------------------------------------------
void ReworkInputString(String InputString)
{
 if(InputString.length()> 40){Serial.printf("Input string too long (max40)\n"); return;}      // If garbage return
 for (int n=0; n<InputString.length()+1; n++)                                                 // remove CR and LF
    if (InputString[n] == 10 || InputString[n]==13) InputString.remove(n,1);
   
 sptext[0] = 0;                                                                               // Empty the sptext string
 
 if(InputString[0] > 31 && InputString[0] <127)                                               // Does the string start with a letter?
  { 
  switch (InputString[0])
   {
    case 'A':
    case 'a': 
             if (InputString.length() >4 )
               {
                InputString.substring(1).toCharArray(Mem.Ssid,InputString.length());
                sprintf(sptext,"SSID set: %s", Mem.Ssid);  
               }
             else sprintf(sptext,"**** Length fault A. Use between 4 and 30 characters ****");
             break;
    case 'B':
    case 'b': 
            if (InputString.length() >4 )
              {  
               InputString.substring(1).toCharArray(Mem.Password,InputString.length());
               sprintf(sptext,"Password set: %s\n Enter @ to reset ESP32 and connect to WIFI and NTP\n WIFI and NTP are turned ON", Mem.Password); 
               Mem.NTPOnOff        = 1;                                                       // NTP On
               Mem.WIFIOnOff       = 1;                                                       // WIFI On  
              }
             else sprintf(sptext,"%s,**** Length fault B. Use between 4 and 40 characters ****",Mem.Password);
             break;   
    case 'C':
    case 'c': 
            if (InputString.length() >4 )
             {  
              InputString.substring(1).toCharArray(Mem.BLEbroadcastName,InputString.length());
              sprintf(sptext,"BLE broadcast name set: %s", Mem.BLEbroadcastName); 
              Mem.BLEOnOff        = 1;                                                        // BLE On
             }
             else sprintf(sptext,"**** Length fault C. Use between 4 and 30 characters ****");
             break;      
    case 'D':
    case 'd':  
             if (InputString.length() == 9 )
               {
                timeinfo.tm_mday = (int) SConstrainInt(InputString,1,3,0,31);
                timeinfo.tm_mon  = (int) SConstrainInt(InputString,3,5,0,12) - 1; 
                timeinfo.tm_year = (int) SConstrainInt(InputString,5,9,2000,9999) - 1900;
                SetDS3231RTCTijd();
                PrintDS3231RTCtijd();                
               }
             else sprintf(sptext,"****\nLength fault . Enter Dddmmyyyy\n****");
             break;
    case 'E':
    case 'e':  
            if (InputString.length() >2 )
             {  
              InputString.substring(1).toCharArray(Mem.Timezone,InputString.length());
              sprintf(sptext,"Timezone set: %s", Mem.Timezone); 
             }
             else sprintf(sptext,"**** Length fault E. Use more than 2 characters ****");
             break;  
    case 'F':
    case 'f':    
             if (InputString.length() == 1)
                {
                 FifteenSecondsTick = !FifteenSecondsTick;
                 if(FifteenSecondsTick) SecondsTicks = false;
                 else Flip.Display_3x1(1, 1,1,0);
                 sprintf(sptext,"15-seconds tick is %s", FifteenSecondsTick?"ON":"OFF" );
                }
               else sprintf(sptext,"**** Length fault.F ****");
              break;    
            
             break;
    case 'G':
    case 'g':          
             break;
    case 'H':
    case 'h':  
             break;        
    case 'I':
    case 'i': 
            SWversion();
            break;
    case 'J':
    case 'j':
             if (InputString.length() == 1)
               {   
                Mem.UseDS3231 = 1 - Mem.UseDS3231; 
                sprintf(sptext,"Use DS3231 is %s", Mem.UseDS3231?"ON":"OFF" );
                lastminute = 99;                                                            // Force a time update                
               }                                
             else sprintf(sptext,"**** Length fault J. ****");
             break; 
    case 'K':
    case 'k':
             if (InputString.length() == 1)
               {    
                 TestLDR = 1 - TestLDR;                                                       // If TestLDR = 1 LDR reading is printed every second instead every 30s
                 sprintf(sptext,"TestLDR: %s \nLoops per second",TestLDR? "On" : "Off");
               }
             else sprintf(sptext,"**** Length fault K. ****");
             break;      
    case 'L':                                                                                 
    case 'l':
   
    case 'M':
    case 'm':
             sprintf(sptext,"**** Length fault M. ****");
             if (InputString.length() == 1)
               {   
                Demo = false; 
                sprintf(sptext,"Demo mode: %s",Demo?"ON":"OFF");   
               }
            if (InputString.length() >1 && InputString.length() < 4 )
              {
                MilliSecondValue = InputString.substring(1,3).toInt()*1000;                  // Flipo to slow to be faster than seconds 
                Demo = true;                                                                  // Toggle Demo mode
                sprintf(sptext,"Demo mode: %s MillisecondTime=%d",Demo?"ON":"OFF", MilliSecondValue); 
              }
             break; 
    case 'N':
    case 'n':
             sprintf(sptext,"**** Length fault N. ****");
             if (InputString.length() == 1 )         Mem.TurnOffLEDsAtHH = Mem.TurnOnLEDsAtHH = 0;
             if (InputString.length() == 5 )
              {
               Mem.TurnOffLEDsAtHH =(byte) InputString.substring(1,3).toInt(); 
               Mem.TurnOnLEDsAtHH  =(byte) InputString.substring(3,5).toInt(); 
              }
             Mem.TurnOffLEDsAtHH = _min(Mem.TurnOffLEDsAtHH, 23);
             Mem.TurnOnLEDsAtHH  = _min(Mem.TurnOnLEDsAtHH, 23); 
             sprintf(sptext,"Display is OFF between %2d:00 and %2d:00", Mem.TurnOffLEDsAtHH,Mem.TurnOnLEDsAtHH );
             break;
    case 'O':
    case 'o':
             sprintf(sptext,"**** Length fault O. ****");
             if(InputString.length() == 1)
               {
                LEDsAreOff = !LEDsAreOff;
                sprintf(sptext,"Display is %s", LEDsAreOff?"OFF":"ON" );
                if(LEDsAreOff)  ClearScreen();                                                // Turn the display off
                else 
                {
                  Tekstprintln(sptext); 
                  lastminute = 99;                                                            // Force a time update
                  Displaytime();                                                              // Turn the display on   
                }
               }
             break;                                                                   
    case 'P':
    case 'p':
             sprintf(sptext,"**** Length fault P. ****");  
             if(InputString.length() == 1)
               {
                Mem.StatusLEDOnOff = !Mem.StatusLEDOnOff;
                if (Mem.StatusLEDOnOff ) SetStatusLED(0,0,0); 
                sprintf(sptext,"StatusLEDs are %s", Mem.StatusLEDOnOff?"ON":"OFF" );               
               }
             break;        

    case 'q':
    case 'Q':   
 
             break;
    case 'R':
    case 'r':
             if (InputString.length() == 1)
               {   
                Reset();
                sprintf(sptext,"\nReset to default values: Done");
                Displaytime();                                                                // Turn on the display with proper time
               }                                
             else sprintf(sptext,"**** Length fault. Enter R ****");
             break;      
    case 'S':                                                                                 // Slope. factor ( 0 - 1) to multiply brighness (0 - 255) with 
    case 's':             
             sprintf(sptext,"**** Length fault S. ****");
             if (InputString.length() == 1)
                {
                  SecondsTicks = !SecondsTicks;
                  if(SecondsTicks) FifteenSecondsTick = false;
                  else Flip.Display_3x1(1, 1,1,0);
                  sprintf(sptext,"Seconds tick is %s", SecondsTicks?"ON":"OFF" );
                }
              break;                    
    case 'T':
    case 't':
//                                                                                            //
             if(InputString.length() == 7)  // T125500
               {
                timeinfo.tm_hour = (int) SConstrainInt(InputString,1,3,0,23);
                timeinfo.tm_min  = (int) SConstrainInt(InputString,3,5,0,59); 
                timeinfo.tm_sec  = (int) SConstrainInt(InputString,5,7,0,59);
              //  SetRTCTime();
              //  Print_RTC_tijd(); 
              // if(Mem.UseDS3231)  
               SetDS3231RTCTijd();
               PrintDS3231RTCtijd();
                SetRTCTime();
                Print_tijd(); 
               }
             else sprintf(sptext,"**** Length fault. Enter Thhmmss ****");
             break;            
    case 'U':                                                                                 // factor to multiply brighness (0 - 255) with 
    case 'u':
 
              break;  
    case 'V':
    case 'v':  
              
             break;      
    case 'W':
    case 'w':
             if (InputString.length() == 1)
               {   
                Mem.WIFIOnOff = 1 - Mem.WIFIOnOff; 
                Mem.ReconnectWIFI = 0;                                                       // Reset WIFI reconnection counter 
                Mem.NTPOnOff = Mem.WIFIOnOff;                                                // If WIFI is off turn NTP also off
                sprintf(sptext,"WIFI is %s after restart", Mem.WIFIOnOff?"ON":"OFF" );
               }                                
             else sprintf(sptext,"**** Length fault. Enter W ****");
             break; 
    case 'X':
    case 'x':
             if (InputString.length() == 1)
               {   
                Mem.NTPOnOff = 1 - Mem.NTPOnOff; 
                sprintf(sptext,"NTP is %s after restart", Mem.NTPOnOff?"ON":"OFF" );
               }                                
             else sprintf(sptext,"**** Length fault. Enter X ****");
             break; 
    case 'Y':
    case 'y':
             if (InputString.length() == 1)
               {   
                Mem.BLEOnOff = 1 - Mem.BLEOnOff; 
                sprintf(sptext,"BLE is %s after restart", Mem.BLEOnOff?"ON":"OFF" );
               }                                
             else sprintf(sptext,"**** Length fault. Enter Y ****");
             break; 
    case 'Z':
    case 'z':
             if (InputString.length() == 1)
               {   
                Mem.UseBLELongString = 1 - Mem.UseBLELongString; 
                sprintf(sptext,"Fast BLE is %s", Mem.UseBLELongString?"ON":"OFF" );
               }                                
             else sprintf(sptext,"**** Length fault. Enter U ****");
             break; 
//--------------------------------------------                                                //        
     case '!':                                                                                // Print the NTP, RTC and DS3231 time
             if (InputString.length() == 1)  PrintAllClockTimes();
             break;       
    case '@':
             if (InputString.length() == 1)
               {   
               Tekstprintln("\n*********\n ESP restarting\n*********\n");
                ESP.restart();   
               }                                
             else sprintf(sptext,"**** Length fault. Enter @ ****");
             break;     
    case '#':
             if (InputString.length() == 1)
               {
                Zelftest = 1 - Zelftest; 
                sprintf(sptext,"Zelftest: %s", Zelftest?"ON":"OFF" ); 
//                Selftest();   
                Displaytime();                                                                // Turn on the display with proper time
               }                                
             else sprintf(sptext,"**** Length fault. Enter # ****");
             break; 
    case '$':
           if (InputString.length() == 1)
               {
                IsDS3231I2Cconnected();
               }
             break; 
    case '%':
             break; 
    case '&':
             sprintf(sptext,"**** Length fault &. ****");                                                                                // Forced get NTP time and update the DS32RTC module
             if (InputString.length() == 1)
              {
               NTP.getTime();                                                                // Force a NTP time update  
               SetDS3231RTCTijd();
               SetRTCTime();    
               PrintAllClockTimes();
               } 
             break;
                      
    case '0':
    case '1':
    case '2':
             sprintf(sptext,"**** Length fault in time digits. Enter hhmmss****");        
             if (InputString.length() == 6)                                                  // For compatibility input with only the time digits
              {
               timeinfo.tm_hour = (int) SConstrainInt(InputString,0,2,0,23);
               timeinfo.tm_min  = (int) SConstrainInt(InputString,2,4,0,59); 
               timeinfo.tm_sec  = (int) SConstrainInt(InputString,4,6,0,59);
               sprintf(sptext,"Time set");  
              //  SetRTCTime();
              //  Print_RTC_tijd(); 
              // if(Mem.UseDS3231)  
               SetDS3231RTCTijd();
               PrintDS3231RTCtijd();
               } 
    default: break;
    }
  }  
 Tekstprintln(sptext); 
 StoreStructInFlashMemory();                                                                  // Update EEPROM                                     
 InputString = "";
}



//--------------------------------------------
// LED CLOCK Control the LEDs on the Nano ESP32
// 0 Low is LED off. Therefore the value is inversed with the ! Not 
//--------------------------------------------
void SetStatusLED(bool Red, bool Green, bool Blue)
{
 digitalWrite(LED_RED,   !Red);                                                                 // !Red (not Red) because 1 or HIGH is LED off
 digitalWrite(LED_GREEN, !Green);
 digitalWrite(LED_BLUE,  !Blue);
}

//--------------------------------------------                                                //
//  DISPLAY
//  Clear the display
//--------------------------------------------
void ClearScreen(void)
{
 Flip.Clear(); 
}
//--------------------------------------------                                                //
// CLOCK Feed the time tothe Flipo's

//--------------------------------------------
void Displaytime()
{ 
  if(LEDsAreOff) return;                                                                       // Display must stay off
  DisplayFlipoTime();                                                                          //
}


//--------------------------------------------                                                //
//  LED Push data in LED strip to commit the changes
//--------------------------------------------
void ShowLeds(void)
{
 
}
//--------------------------- Time functions --------------------------

//--------------------------------------------                                                //
// RTC Get time from NTP cq internal ESP32 RTC 
// and store it in timeinfo struct
// return local time in unix time format
//--------------------------------------------
time_t GetTijd(byte printit)
{
 time_t now;
 
 if (Mem.UseDS3231) GetDS3231RTCTijd(0);                                                      // If the DS3231 is attached and used get its time in timeinfo struct
 else
    { 
     if(Mem.NTPOnOff)  getLocalTime(&timeinfo);                                               // If NTP is running get the local time
     else { time(&now); localtime_r(&now, &timeinfo);}                                        // Else get the time from the internal RTC 
    }
 if (printit)  Print_RTC_tijd();                                                              // 
  localtime(&now);
 return now;                                                                                  // Return the unixtime in seconds
}
//--------------------------------------------                                                //
// RTC prints the ESP32 internal RTC time to serial
//--------------------------------------------
void Print_RTC_tijd(void)
{
 sprintf(sptext,"%02d/%02d/%04d %02d:%02d:%02d ", 
     timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year+1900,
     timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
 Tekstprint(sptext);
}
//--------------------------------------------                                                //
// RTC prints the external RTC klok time to serial
//--------------------------------------------
void Print_RTCmodule_time(void)
{
  sprintf(sptext,"%02d/%02d/%04d %02d:%02d:%02d ", 
     RTCtm.tm_mday,RTCtm.tm_mon+1,RTCtm.tm_year+1900,
     RTCtm.tm_hour,RTCtm.tm_min,RTCtm.tm_sec);
 Tekstprint(sptext); 
}

//--------------------------------------------                                                //
// DS3231 Init module
//--------------------------------------------
void InitDS3231Mod(void)
{
 DS3231Installed = IsDS3231I2Cconnected();                                                    // Check if DS3231 is connected and working   
 sprintf(sptext,"DS3231 RTC module %s installed", DS3231Installed?"IS":"NOT"); 
 Tekstprintln(sptext);                                                                 
}
//--------------------------------------------                                                //
// DS3231 check for I2C connection
// DS3231_I2C_ADDRESS (= often 0X68) = DS3231 module
//--------------------------------------------
 bool IsDS3231I2Cconnected(void)
 {
  bool DS3231Found = false;
  for (byte i = 1; i < 120; i++)
  {
   Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0)                                                       
      {
      sprintf(sptext,"Found I2C address: 0X%02X", i); Tekstprintln(sptext);  
      if( i== DS3231_I2C_ADDRESS) DS3231Found = true;
      } 
  }
  return DS3231Found;   
  }
//--------------------------------------------                                                //
// DS3231 prints time to serial
// reference https://adafruit.github.io/RTClib/html/class_r_t_c___d_s3231.html
//--------------------------------------------
void PrintDS3231RTCtijd(void)
{
GetDS3231RTCTijd(0);
sprintf(sptext,"%02d/%02d/%04d %02d:%02d:%02d ", 
      RTCtm.tm_mday,RTCtm.tm_mon+1,RTCtm.tm_year+1900, RTCtm.tm_hour,RTCtm.tm_min,RTCtm.tm_sec);
 Tekstprint(sptext);
}
//--------------------------------------------                                                //
// DS3231 Get time from DS3231 and stores it in 
// timeinfo struct the clock uses for displaying the time
//--------------------------------------------
void GetDS3231RTCTijd(byte printit)
{
ReadRX8025Ttime();
//ReadDS3231time();
 if (printit)  Print_RTCmodule_time(); 
}

//--------------------------------------------                                                //
// DS3231 Set time in module and print it
//--------------------------------------------
void SetDS3231RTCTijd(void)
{ 
SetRX8025Ttime();
//SetDS3231time();
}

//--------------------------------------------                                                //
// DS3231 Get temperature from module
//--------------------------------------------
float GetDS3231Temp(void)
{
 byte tMSB, tLSB;
 float temp3231;
 
  Wire.beginTransmission(DS3231_I2C_ADDRESS);                                                 // Temp registers (11h-12h) get updated automatically every 64s
  Wire.write(0x11);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 2);
 
  if(Wire.available()) 
  {
    tMSB = Wire.read();                                                                       // 2's complement int portion
    tLSB = Wire.read();                                                                       // fraction portion 
    temp3231 = (tMSB & 0b01111111);                                                           // do 2's math on Tmsb
    temp3231 += ( (tLSB >> 6) * 0.25 ) + 0.5;                                                 // only care about bits 7 & 8 and add 0.5 to round off to integer   
  }
  else   {temp3231 = -273; }  
  return (temp3231);
}

//--------------------------------------------                                                //
// DS3231  Convert normal decimal numbers to binary coded decimal and vise versa
//--------------------------------------------
byte decToBcd(byte val){ return( (val/10*16) + (val%10) );}

byte bcdToDec(byte val){ return( (val/16*10) + (val%16) );}


/*----------------------------------------------------------------------*
 * Decimal-to-Dedicated format conversion - RTC datasheet page 12       *
 * Sunday 0x01,...Wednesday 0x08,...Saturday 0x40                       *
 *----------------------------------------------------------------------*/
uint8_t wday2bin(uint8_t wday)
{
  return (wday - 1);
}

/*----------------------------------------------------------------------*
 * Dedicated-to-Decimal format conversion - RTC datasheet page 12       *
 * Sunday 1, Monday 2, Tuesday 3...                                     *
 *----------------------------------------------------------------------*/
uint8_t bin2wday(uint8_t wday)
{
  for(int i = 0; i < 7; i++) if((wday >> i) == 1) wday = i + 1;
  return wday;
}

/*----------------------------------------------------------------------*
 * Reads the current time from the RTC in RTCtm and returns it in a tmElements_t *
 * structure. Returns the I2C status (zero if successful).              *
 *----------------------------------------------------------------------*/
uint8_t ReadRX8025Ttime(void)
{
  Wire.beginTransmission(RX8025T_ADDR);
  Wire.write((uint8_t)RX8025T_SECONDS);
  if ( uint8_t e = Wire.endTransmission() ) return e;
  //request 7 bytes (secs, min, hr, dow, date, mth, yr)
  Wire.requestFrom(RX8025T_ADDR, 7);  //tmNbrFields);
  RTCtm.tm_sec    = bcdToDec(Wire.read());   
  RTCtm.tm_min    = bcdToDec(Wire.read());
  RTCtm.tm_hour   = bcdToDec(Wire.read());
  RTCtm.tm_wday   = bcdToDec(Wire.read());
// RTCtm.tm_wday = bin2wday(Wire.read());
 RTCtm.tm_mday    = bcdToDec(Wire.read());
 RTCtm.tm_mon     = bcdToDec(Wire.read());
 RTCtm.tm_year    = bcdToDec(Wire.read());
 timeinfo = RTCtm;
  return 0;
}


/*----------------------------------------------------------------------*
 * Sets the RTC's time from a tmElements_t structure and clears the     *
 * oscillator stop flag (OSF) in the Control/Status register.           *
 * Returns the I2C status (zero if successful).                         *
 *----------------------------------------------------------------------*/
uint8_t SetRX8025Ttime(void)
{
  Wire.beginTransmission(RX8025T_ADDR);
  Wire.write((uint8_t)RX8025T_SECONDS);
  Wire.write(decToBcd(timeinfo.tm_sec));
  Wire.write(decToBcd(timeinfo.tm_min));
  Wire.write(decToBcd(timeinfo.tm_hour));
  Wire.write(wday2bin(timeinfo.tm_wday));
//  Wire.write(decToBcd(timeinfo.tm_wday));
  Wire.write(decToBcd(timeinfo.tm_mday));
  Wire.write(decToBcd(timeinfo.tm_mon)); 
  Wire.write(decToBcd(timeinfo.tm_year));     
  uint8_t ret =  Wire.endTransmission();
  return ret;
}

//--------------------------------------------                                                //
// DS3231 Set time in module DS3231
//--------------------------------------------
void SetDS3231time(void)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x0E);                                                                           // Select register
  Wire.write(0b00011100);                                                                     // Write register bitmap, bit 7 is /EOS
  Wire.write(decToBcd(timeinfo.tm_sec));
  Wire.write(decToBcd(timeinfo.tm_min));
  Wire.write(decToBcd(timeinfo.tm_hour));
 // i2cWrite(wday2bin(timeinfo.tm_wday));
  Wire.write(decToBcd(timeinfo.tm_wday));
  Wire.write(decToBcd(timeinfo.tm_mday));
  Wire.write(decToBcd(timeinfo.tm_mon)); 
  Wire.write(decToBcd(timeinfo.tm_year)); 
  Wire.endTransmission();
}

//--------------------------------------------                                                //
// DS3231 reads time in module DS3231
//--------------------------------------------
void ReadDS3231time(void)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);                                                                              // Set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);                                                    // Read 7 times
  
  RTCtm.tm_sec    = bcdToDec(Wire.read());   
  RTCtm.tm_min    = bcdToDec(Wire.read());
  RTCtm.tm_hour   = bcdToDec(Wire.read());
  RTCtm.tm_wday   = bcdToDec(Wire.read());
  RTCtm.tm_mday   = bcdToDec(Wire.read());
  RTCtm.tm_mon    = bcdToDec(Wire.read());
  RTCtm.tm_year   = bcdToDec(Wire.read());
  timeinfo = RTCtm;
}
//--------------------------------------------                                                //
// DS3231 Diplays time of module DS3231
//--------------------------------------------  


void DisplayDS3231Time()
{
 //*****************  OBSOLETE ReadDS3231time();            // Retrieve data from DS3231
}
//--------------------------------------------                                                //
// NTP print the NTP time for the timezone set 
//--------------------------------------------
void PrintNTP_tijd(void)
{
 sprintf(sptext,"%s  ", NTP.getTimeDateString());  
 Tekstprint(sptext);              // 17/10/2022 16:08:15
}

//--------------------------------------------                                                //
// NTP print the NTP UTC time 
//--------------------------------------------
void PrintUTCtijd(void)
{
 time_t tmi;
 struct tm* UTCtime;
 time(&tmi);
 UTCtime = gmtime(&tmi);
 sprintf(sptext,"UTC: %02d:%02d:%02d %02d-%02d-%04d  ", 
     UTCtime->tm_hour,UTCtime->tm_min,UTCtime->tm_sec,
     UTCtime->tm_mday,UTCtime->tm_mon+1,UTCtime->tm_year+1900);
 Tekstprint(sptext);   
}

//--------------------------------------------                                                //
// NTP processSyncEvent 
//--------------------------------------------
void processSyncEvent (NTPEvent_t ntpEvent) 
{
 switch (ntpEvent.event) 
    {
        case timeSyncd:
        case partlySync:
        case syncNotNeeded:
        case accuracyError:
            sprintf(sptext,"[NTP-event] %s\n", NTP.ntpEvent2str (ntpEvent));
            Tekstprint(sptext);
            break;
        default:
            break;
    }
}
//--------------------------------------------                                                //
// RTC Fill sptext with time
//--------------------------------------------
void Print_tijd(){ Print_tijd(2);}                                                            // print with linefeed
void Print_tijd(byte format)
{
 sprintf(sptext,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
 switch (format)
 {
  case 0: break;
  case 1: Tekstprint(sptext); break;
  case 2: Tekstprintln(sptext); break;  
 }
}

//--------------------------------------------                                                //
// RTC Set time from global timeinfo struct
// Check if values are within range
//--------------------------------------------
void SetRTCTime(void)
{ 
 time_t t = mktime(&timeinfo);  // t= unixtime
 sprintf(sptext, "Setting time: %s", asctime(&timeinfo));    Tekstprintln(sptext);
 struct timeval now = { .tv_sec = t , .tv_usec = 0};
 settimeofday(&now, NULL);
 GetTijd(0);                                                                                  // Synchronize time with RTC clock
 Displaytime();
 Print_tijd();
}

//--------------------------------------------                                                //
// Print all the times available 
//--------------------------------------------
void PrintAllClockTimes(void)
{
 Tekstprint(" Clock time: ");
 GetTijd(1);
 Tekstprint("\n   NTP time: ");
 PrintNTP_tijd();
 Tekstprint("\nRTCmod time: ");
 PrintDS3231RTCtijd();
 Tekstprintln(""); 
}
//                                                                                            //
// ------------------- End  Time functions 

//--------------------------------------------                                                //
//  CLOCK Convert Hex to uint32
//--------------------------------------------
uint32_t HexToDec(String hexString) 
{
 uint32_t decValue = 0;
 int nextInt;
 for (uint8_t i = 0; i < hexString.length(); i++) 
  {
   nextInt = int(hexString.charAt(i));
   if (nextInt >= 48 && nextInt <= 57)  nextInt = map(nextInt, 48, 57, 0, 9);
   if (nextInt >= 65 && nextInt <= 70)  nextInt = map(nextInt, 65, 70, 10, 15);
   if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
   nextInt = constrain(nextInt, 0, 15);
   decValue = (decValue * 16) + nextInt;
  }
 return decValue;
}
//------------------------------------------------------------------------------
// CLOCK Demo mode
//------------------------------------------------------------------------------
//                                                                                            //
void Demomode(void)
{
 if ( millis() - msTick == 10)   digitalWrite(secondsPin,LOW);                                // Turn OFF the second on pin 13
 if ( millis() - msTick >= MilliSecondValue)                                                  // Flash the onboard Pin 13 Led so we know something is happening
 {    
  msTick = millis();                                                                          // second++; 
  digitalWrite(secondsPin,HIGH);                                                              // Turn ON the second on pin 13
  if( ++timeinfo.tm_sec >59) { timeinfo.tm_sec = 0; }
  if( ++timeinfo.tm_min >59) { timeinfo.tm_min = 0; timeinfo.tm_sec = 0; timeinfo.tm_hour++;}
  if( timeinfo.tm_hour >24) timeinfo.tm_hour = 0;                                             // If hour is after 12 o'clock 
  Displaytime();
  Tekstprintln("");
  SerialCheck();
 }
}

//--------------------------------------------                                                //
// BLE 
// SendMessage by BLE Slow in packets of 20 chars
// or fast in one long string.
// Fast can be used in IOS app BLESerial Pro
//------------------------------
void SendMessageBLE(std::string Message)
{
 if(deviceConnected) 
   {
    if (Mem.UseBLELongString)                                                                 // If Fast transmission is possible
     {
      pTxCharacteristic->setValue(Message); 
      pTxCharacteristic->notify();
      delay(10);                                                                              // Bluetooth stack will go into congestion, if too many packets are sent
     } 
   else                                                                                       // Packets of max 20 bytes
     {   
      int parts = (Message.length()/20) + 1;
      for(int n=0;n<parts;n++)
        {   
         pTxCharacteristic->setValue(Message.substr(n*20, 20)); 
         pTxCharacteristic->notify();
         delay(40);                                                                           // Bluetooth stack will go into congestion, if too many packets are sent
        }
     }
   } 
}
//-----------------------------
// BLE Start BLE Classes
//------------------------------
class MyServerCallbacks: public BLEServerCallbacks 
{
 void onConnect(BLEServer* pServer) {deviceConnected = true; };
 void onDisconnect(BLEServer* pServer) {deviceConnected = false;}
};

class MyCallbacks: public BLECharacteristicCallbacks 
{
 void onWrite(BLECharacteristic *pCharacteristic) 
  {
   std::string rxValue = pCharacteristic->getValue();
   ReceivedMessageBLE = rxValue + "\n";
//   if (rxValue.length() > 0) {for (int i = 0; i < rxValue.length(); i++) printf("%c",rxValue[i]); }
//   printf("\n");
  }  
};
//--------------------------------------------                                                //
// BLE Start BLE Service
//------------------------------
void StartBLEService(void)
{
 BLEDevice::init(Mem.BLEbroadcastName);                                                       // Create the BLE Device
 pServer = BLEDevice::createServer();                                                         // Create the BLE Server
 pServer->setCallbacks(new MyServerCallbacks());
 BLEService *pService = pServer->createService(SERVICE_UUID);                                 // Create the BLE Service
 pTxCharacteristic                     =                                                      // Create a BLE Characteristic 
     pService->createCharacteristic(CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY);                 
 BLECharacteristic * pRxCharacteristic = 
     pService->createCharacteristic(CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE);
 pRxCharacteristic->setCallbacks(new MyCallbacks());
 pService->start(); 
 BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
 pAdvertising->addServiceUUID(SERVICE_UUID); 
 pServer->start();                                                                            // Start the server  Nodig??
 pServer->getAdvertising()->start();                                                          // Start advertising
 TekstSprint("BLE Waiting a client connection to notify ...\n"); 
}
//                                                                                            //
//-----------------------------
// BLE  CheckBLE
//------------------------------
void CheckBLE(void)
{
 if(!deviceConnected && oldDeviceConnected)                                                   // Disconnecting
   {
    delay(300);                                                                               // Give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();                                                              // Restart advertising
    TekstSprint("Start advertising\n");
    oldDeviceConnected = deviceConnected;
   }
 if(deviceConnected && !oldDeviceConnected)                                                   // Connecting
   { 
    oldDeviceConnected = deviceConnected;
    SWversion();
   }
 if(ReceivedMessageBLE.length()>0)
   {
    SendMessageBLE(ReceivedMessageBLE);
    String BLEtext = ReceivedMessageBLE.c_str();
    ReceivedMessageBLE = "";
    ReworkInputString(BLEtext); 
   }
}

//--------------------------------------------                                                //
// WIFI WIFIEvents
//--------------------------------------------
void WiFiEvent(WiFiEvent_t event)
{
  sprintf(sptext,"[WiFi-event] event: %d  : ", event); 
  Tekstprint(sptext);
  WiFiEventInfo_t info;
    switch (event) 
     {
        case ARDUINO_EVENT_WIFI_READY: 
            Tekstprintln("WiFi interface ready");
            break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            Tekstprintln("Completed scan for access points");
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            Tekstprintln("WiFi client started");
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            Tekstprintln("WiFi clients stopped");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Tekstprintln("Connected to access point");
            break;
       case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Mem.ReconnectWIFI++;
            sprintf(sptext, "Disconnected from station, attempting reconnection for the %d time", Mem.ReconnectWIFI);
            Tekstprintln(sptext);
            sprintf(sptext,"WiFi lost connection.");                                          // Reason: %d",info.wifi_sta_disconnected.reason); 
            Tekstprintln(sptext);
            WiFi.reconnect();
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            Tekstprintln("Authentication mode of access point has changed");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            sprintf(sptext, "Obtained IP address: %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
            Tekstprintln(sptext);
            WiFiGotIP(event,info);
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            Tekstprintln("Lost IP address and IP address is reset to 0");
            break;
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
 //           txtstr = WiFi.SSID().c_str();
//            sprintf(sptext, "WPS Successfull, stopping WPS and connecting to: %s: ", txtstr);
 //           Tekstprintln(sptext);
//            wpsStop();
//            delay(10);
//            WiFi.begin();
            break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
            Tekstprintln("WPS Failed, retrying");
//            wpsStop();
//            wpsStart();
            break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            Tekstprintln("WPS Timedout, retrying");
//            wpsStop();
//            wpsStart();
            break;
        case ARDUINO_EVENT_WPS_ER_PIN:
 //           txtstr = wpspin2string(info.wps_er_pin.pin_code).c_str();
//            sprintf(sptext,"WPS_PIN = %s",txtstr);
//            Tekstprintln(sptext);
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            Tekstprintln("WiFi access point started");
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            Tekstprintln("WiFi access point  stopped");
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Tekstprintln("Client connected");
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            sprintf(sptext,"Client disconnected.");                                            // Reason: %d",info.wifi_ap_stadisconnected.reason); 
            Tekstprintln(sptext);

//          Serial.print("WiFi lost connection. Reason: ");
//          Tekstprintln(info.wifi_sta_disconnected.reason);
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Tekstprintln("Assigned IP address to client");
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            Tekstprintln("Received probe request");
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            Tekstprintln("AP IPv6 is preferred");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            Tekstprintln("STA IPv6 is preferred");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP6:
            Tekstprintln("Ethernet IPv6 is preferred");
            break;
        case ARDUINO_EVENT_ETH_START:
            Tekstprintln("Ethernet started");
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Tekstprintln("Ethernet stopped");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Tekstprintln("Ethernet connected");
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Tekstprintln("Ethernet disconnected");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Tekstprintln("Obtained IP address");
            WiFiGotIP(event,info);
            break;
        default: break;
    }
}
//--------------------------------------------                                                //
// WIFI GOT IP address 
//--------------------------------------------
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  WIFIConnected = 1;
//       Displayprintln("WIFI is On"); 
 if(Mem.WIFIOnOff) WebPage();                                                                 // Show the web page if WIFI is on
 if(Mem.NTPOnOff)
   {
    NTP.setTimeZone(Mem.Timezone);                                                            // TZ_Europe_Amsterdam); //\TZ_Etc_GMTp1); // TZ_Etc_UTC 
    NTP.begin();                                                                              // https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
    Tekstprintln("NTP is On"); 
    // Displayprintln("NTP is On");                                                           // Print the text on the display
   } 
}

//--------------------------------------------                                                //
// WIFI WEBPAGE 
//--------------------------------------------
void StartWIFI_NTP(void)
{
// WiFi.disconnect(true);
// WiFi.onEvent(WiFiEvent);   // Using WiFi.onEvent interrupts and crashes IL9341 screen display while writing the screen
// Examples of different ways to register wifi events
                         //  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
                         //  WiFiEventId_t eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
                         //    {Serial.print("WiFi lost connection. Reason: ");  Serial.println(info.wifi_sta_disconnected.reason); }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
// if(strlen (Mem.Ssid) <5 || strlen(Mem.Password) <6) { AskSSIDPW(); return;}

 
 WiFi.mode(WIFI_STA);
 WiFi.begin(Mem.Ssid, Mem.Password);
// WhiteOverRainbow(5, 5, 5);                                                                    // WhiteOverRainbow(uint32_t wait, uint8_t whiteSpeed, uint32_t whiteLength ) 
 //StartLeds();                                                                                // Let the WIFI connect and show in the meantime the LEDs

 if (WiFi.waitForConnectResult() != WL_CONNECTED) 
      { 
       if(Mem.WIFINoOfRestarts == 0)                                                          // Try once to restart the ESP and make a new WIFI connection
       {
          Mem.WIFINoOfRestarts = 1;
          StoreStructInFlashMemory();                                                         // Update Mem struct   
          ESP.restart(); 
       }
       else
       {  
         // Displayprintln("WiFi Failed!");                                                   // Print the text on the display
         Tekstprintln("WiFi Failed! Enter @ to retry"); 
         WIFIConnected = 0;       
         return;
       }
      }
 else 
      {
       Tekstprint("Web page started\n");
       sprintf(sptext, "IP Address: %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
       // Displayprintln(sptext);
       Tekstprintln(sptext); 
       WIFIConnected = 1;
       Mem.WIFINoOfRestarts = 0;
       StoreStructInFlashMemory();                                                            // Update Mem struct   
       // Displayprintln("WIFI is On"); 
       // AsyncElegantOTA.begin(&server);                                                     // Start ElegantOTA  old version
       ElegantOTA.begin(&server);                                                             // Start ElegantOTA  new version in 2023  
       // if compile error see here :https://docs.elegantota.pro/async-mode/
                                                                                              // Open ElegantOTA folder and then open src folder
                                                                                              // Locate the ELEGANTOTA_USE_ASYNC_WEBSERVER macro in the ElegantOTA.h file, and set it to 1:
                                                                                              // #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
                                                                                              // Save the changes to the ElegantOTA.h file.   
       if(Mem.NTPOnOff)
          {
           NTP.setTimeZone(Mem.Timezone);                                                     // TZ_Europe_Amsterdam); //\TZ_Etc_GMTp1); // TZ_Etc_UTC 
           NTP.begin();                                                                       // https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
           Tekstprintln("NTP is On"); 
           // Displayprintln("NTP is On");                                                    // Print the text on the display
          }
//       PrintIPaddressInScreen();
      }
 if(Mem.WIFIOnOff) WebPage();                                                                 // Show the web page if WIFI is on
}


//--------------------------------------------                                                //
// WIFI WEBPAGE 
//--------------------------------------------
void WebPage(void) 
{
 server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)                                  // Send web page with input fields to client
          { request->send_P(200, "text/html", index_html);  }    );
 server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request)                              // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
       { 
        String inputMessage;    String inputParam;
        if (request->hasParam(PARAM_INPUT_1))                                                 // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
           {
            inputMessage = request->getParam(PARAM_INPUT_1)->value();
            inputParam = PARAM_INPUT_1;
           }
        else 
           {
            inputMessage = "";    //inputMessage = "No message sent";
            inputParam = "none";
           }  
  //  error oplossen       sprintf(sptext,"%s",inputMessage);    Tekstprintln(sptext); //format '%s' expects argument of type 'char*', but argument 3 has type 'String' [-Werror=format=]

        ReworkInputString(inputMessage);
        request->send_P(200, "text/html", index_html);
       }   );
 server.onNotFound(notFound);
 server.begin();
}

//--------------------------------------------                                                //
// WIFI WEBPAGE 
//--------------------------------------------
void notFound(AsyncWebServerRequest *request) 
{
  request->send(404, "text/plain", "Not found");
}





//                                                                                            //
