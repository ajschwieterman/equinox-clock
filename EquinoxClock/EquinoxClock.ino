#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

#ifndef STASSID
#define STASSID "Tangy Elephants"
#define STAPSK  "Tintlab3A"
#define BUTTON_PIN  12
#define CYCLONE_PIN 13  
#define PHOTOCELL_PIN A0
#define LED_PIN    14
#define LED_COUNT 240  //LED_COUNT must be divisible by NUMBER_OF_MINUTES
#define NUMBER_OF_MINUTES 60
#define NUMBER_OF_LEDS_PER_SECOND 2
#define NUMBER_OF_LEDS_PER_MINUTE 4
#define NUMBER_OF_LEDS_PER_HOUR 8
#define EEPROM_SIZE 7
#endif

const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };
enum  ClockMode {NORMAL, INVERSE, PROGRAM, NOTIFICATION};
const char* ssid = STASSID;
const char* password = STAPSK;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
const long utcOffsetInSeconds = -18000;  //For UTC -5.00 : -5 * 60 * 60
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
ESP8266WebServer server;
int pixelOffset = 118;
int hours = 0;
int minutes = 0;
int seconds = 0;
int pureHours = 0;
int pureMinutes = 0;
int pureSeconds = 0;
boolean inDaylightSavingsTime;
unsigned long currentTime = 0;
unsigned long startTime = 0;
int numberOfPixlesPerMinute = LED_COUNT / NUMBER_OF_MINUTES;
uint32_t clockColors[LED_COUNT];
uint32_t clockPreviousColors[LED_COUNT];
uint16_t hourColor;
uint16_t minuteColor;
uint16_t secondColor;
int brightness = 0;
int buttonState = 0;
int previousButtonState = 0;
int numberOfSteps = 0;
int stepIndex = 0;
boolean performReset = false;
uint32_t previousColor;
uint32_t newColor;
uint8_t r;
uint8_t g;
uint8_t b;
ClockMode clockMode;
int notify_flash;
int notify_duration;
uint32_t notify_color;
boolean up = true;

void setup() {
  //Start the serial monitor
  //Serial.begin(115200);

  //Set the pin mode for the mode button
  pinMode(BUTTON_PIN, INPUT);

  //Start the NeoPixel service
  strip.begin();

  //Start the NTP time client service
  timeClient.begin();

  //Start the EEPROM service
  EEPROM.begin(EEPROM_SIZE);
    
  //Do some initializing
  boolean isMemoryDefaulted = true;
  for (int i = 0; i < EEPROM_SIZE; i++) {
    if (EEPROM.read(i) != 0xFF) {
      isMemoryDefaulted = false;
      break;
    }
  }
  if (isMemoryDefaulted) {
    initializeColors();
  }
  inDaylightSavingsTime = (EEPROM.read(6) == 0x01);
  clockMode = NORMAL;
  
  //Set the colors for the hour, minute, and second pixels
  hourColor = (EEPROM.read(0) << 8) | EEPROM.read(1);
  minuteColor = (EEPROM.read(2) << 8) | EEPROM.read(3);
  secondColor = (EEPROM.read(4) << 8) | EEPROM.read(5);

  //Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  //Trigger normal mode
  server.on("/normal", [](){
    server.send(200, "text/plain", "Normal mode initiated");
    clockMode = NORMAL;
  });

  //Trigger program mode
  server.on("/program", [](){
    server.send(200, "text/plain", "Program mode initiated");
    clockMode = PROGRAM;
    numberOfSteps = 50;
    stepIndex = 0;
    up = true;
  });

    //Trigger equinox json sensors
  server.on("/equinox", [](){
    StaticJsonDocument<500> doc;
    //readAmbientBrightness();

    JsonArray illumValues = doc.createNestedArray("brightness");
    illumValues.add(brightness);  

    
    // Write JSON document
    String json;
    serializeJsonPretty(doc, json);
    server.send(200, "application/json", json);
  });

    //override mode with notify service
  server.on("/notify", [](){
    clockMode = NOTIFICATION;
    //numberOfSteps = 50;
    stepIndex = 0;
    up = true;
    String message = "Number of args received:";
    message += server.args();            //Get number of parameters
    message += "\n";                            //Add a new line
    
    for (int i = 0; i < server.args(); i++) {
      if (server.argName(i)== "duration"){
        notify_duration = millis() + server.arg(i).toInt();
        
      } else if (server.argName(i) ==  "flash"){
        notify_flash = server.arg(i).toInt();
      }
      else if (server.argName(i) == "color"){        
        notify_color = server.arg(i).toInt(); //32bit color
      } 
      message += "Arg n�" + (String)i + " �> ";   //Include the current iteration value
      message += server.argName(i) + ": ";     //Get the name of the parameter
      message += server.arg(i) + "\n";              //Get the value of the parameter
    } 
    server.send(200, "text/plain", message);       //Response to the HTTP request
    
    
    
  });

  

  //Start the web server service
  server.begin();

  //Method to call when programming mode has started
  ArduinoOTA.onStart([]() {
    colorWipe(strip.Color(0, 0, 0));
  });

  //Method to call when programming mode has ended
  ArduinoOTA.onEnd([]() {
    readAmbientBrightness();
    colorWipe(strip.Color(0, brightness, 0));
  });

  //Method to call while programming
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    for (int ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
      strip.setPixelColor(ledIndex, (ledIndex < map((progress / (total / 100)), 0, 100, 0, LED_COUNT)) ? strip.Color(30, 30, 30) : strip.Color(0, 0, 0));
    }
    strip.show();
  });

  //Method to call when an error occurs when programming
  ArduinoOTA.onError([](ota_error_t error) {
    readAmbientBrightness();
    colorWipe(strip.Color(brightness, 0, 0));
  });

  //Start the over-the-air update service
  ArduinoOTA.begin();
}

void loop() {  
  //Monitor for any HTTP requests
  server.handleClient();
  
  //Get the ambient brightness level and adjust the LED brightness
  readAmbientBrightness();

  //Monitor the mode button to change the clock's mode
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != previousButtonState) {
    if (buttonState == HIGH) {
      switch (clockMode) {
        case NORMAL:
          clockMode = PROGRAM;
          numberOfSteps = 50;
          stepIndex = 0;
          up = true;
          break;
        case PROGRAM:
          clockMode = NORMAL;
          break;
      }
    }
    delay(50);
  }
  previousButtonState = buttonState;

  //Switch between the various modes
  switch (clockMode) {
    case NORMAL:
    case INVERSE:
      //Get the current time
      timeClient.update();
      currentTime = millis();

      //Did the time change?
      if (pureSeconds != timeClient.getSeconds()) {

        //Save the color information in EEPROM every hour
        if (pureHours != ((timeClient.getHours() + daylightSavingsTimeAdjustment()) % 12)) {
          EEPROM.write(0, (hourColor & 0xFF00) >> 8);
          EEPROM.write(1, (hourColor & 0xFF));
          EEPROM.write(2, (minuteColor & 0xFF00) >> 8);
          EEPROM.write(3, (minuteColor & 0xFF));
          EEPROM.write(4, (secondColor & 0xFF00) >> 8);
          EEPROM.write(5, (secondColor & 0xFF));
          EEPROM.commit();
        }
        
        //Update local time variables
        pureHours = (timeClient.getHours() + daylightSavingsTimeAdjustment() % 12);
        pureMinutes = timeClient.getMinutes();
        pureSeconds = timeClient.getSeconds();
        startTime = currentTime;
    
        //Change colors
        hourColor = (hourColor + 1) & 0xFFFF;
        minuteColor = (minuteColor + 1) & 0xFFFF;
        secondColor = (secondColor + 1) & 0xFFFF;
      }

      //Set the fading index and number of fading steps
      stepIndex = min((int)(currentTime - startTime), 999);
      numberOfSteps = 1000 / numberOfPixlesPerMinute;

      //Save the previous clock color information
      if ((stepIndex % numberOfSteps) < (numberOfSteps / 2) && performReset) {
        for (int i = 0; i < LED_COUNT; i++) {
          clockPreviousColors[i] = clockColors[i];
        }
        performReset = false;
      } else if ((stepIndex % numberOfSteps) > (numberOfSteps / 2) && !performReset) {
        performReset = true;
      }

      //Get the location of the hours, minutes, and seconds
      hours = ((pureHours % 12) * 5 * numberOfPixlesPerMinute) + (pureMinutes * numberOfPixlesPerMinute / 12);
      minutes = (pureMinutes * numberOfPixlesPerMinute) + (pureSeconds * numberOfPixlesPerMinute / NUMBER_OF_MINUTES);
      seconds = (pureSeconds * numberOfPixlesPerMinute) + (stepIndex / numberOfSteps);

      //Reset the colors of the clock
      for (int index = 0; index < LED_COUNT; index++) {
        if (inInversionMode()) {
          clockColors[index] = strip.ColorHSV(0x0000, 0x00, brightness);
        } else {
          clockColors[index] = strip.ColorHSV(0x0000, 0x00, 0x00);
        }
      }
      
      //Set the colors of the clock
      setLedsColors(hours, NUMBER_OF_LEDS_PER_HOUR, hourColor);
      setLedsColors(minutes, NUMBER_OF_LEDS_PER_MINUTE, minuteColor);
      setLedsColors(seconds, NUMBER_OF_LEDS_PER_SECOND, secondColor);
      
      //Display the colors on the clock
      for (int ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
        previousColor = clockPreviousColors[ledIndex];
        newColor = clockColors[ledIndex];
        r = interpolate(Red(previousColor), Red(newColor), stepIndex % numberOfSteps, numberOfSteps);
        g = interpolate(Green(previousColor), Green(newColor), stepIndex % numberOfSteps, numberOfSteps);
        b = interpolate(Blue(previousColor), Blue(newColor), stepIndex % numberOfSteps, numberOfSteps);
        strip.setPixelColor(ledIndex, strip.Color(r, g, b));
      }
      strip.show();
      break;
    case PROGRAM:
      //Display the colors on the clock
      colorWipe(strip.Color(0, 0, map(stepIndex, 0, numberOfSteps, 0x00, brightness)));
      delay(10);

      //Increment or decrement the step index based on the fading direction
      if (up) {
        stepIndex++;
      } else {
        stepIndex--;
      }

      //Determine if it is time to change the fading direction
      if (stepIndex == numberOfSteps) {
        up = false;
      } else if (stepIndex == 1) {
        up = true;
      }

      //Handle incoming over-the-air updates
      ArduinoOTA.handle();
      break;
    case NOTIFICATION:
      //Display the colors on the clock
      if (notify_flash > 0){
        colorWipe(strip.Color(map(stepIndex, 0, notify_flash, 0x00, Red(notify_color)), map(stepIndex, 0, notify_flash, 0x00, Green(notify_color)),map(stepIndex, 0, notify_flash, 0x00, Blue(notify_color))));
      } else{
        colorWipe(strip.Color(  Red(notify_color),  Green(notify_color),  Blue(notify_color)));
      }
      delay(10);

      //Increment or decrement the step index based on the fading direction
      if (up) {
        stepIndex++;
      } else {
        stepIndex--;
      }

      //Determine if it is time to change the fading direction
      if (stepIndex == notify_flash) {
        up = false;
      } else if (stepIndex == 1) {
        up = true;
      }

      if (millis() >= notify_duration){
        clockMode = NORMAL;
      }
      break;
      
  }
}

/**
 * Get the analog read from the photocell.
 */
void readAmbientBrightness() {
  brightness = max(10, (int)(pgm_read_byte(&gamma8[map(analogRead(PHOTOCELL_PIN), 0, 1024, 0, 255)])));
}

/**
 * Check if the clock is in inversion mode.
 * @return True if the clock is in inversion mode; False otherwise
 */
boolean inInversionMode() {
  return (clockMode == INVERSE);
}

/**
 *
 */
uint8_t interpolate(int previousValue, int newValue, int index, int steps) {
  return (((previousValue * (steps - index)) + (newValue * index)) / steps) & 0xFF;
}

/**
 * 
 */
void colorWipe(uint32_t color) {
  for (int ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
    strip.setPixelColor(ledIndex, color);
  }
  strip.show();
}

/**
 * Set the LED color for the LED strip for a time unit.
 * 
 * @param timeValue The current time value for a time unit.
 * @param numberOfLeds The number of LEDs that represent the time unit.
 * @param color The color to represent the time unit on the clock.
 */
void setLedsColors(int timeValue, int numberOfLeds, uint16_t color) {
  //Variables
  int ledIndex;
  int leds[numberOfLeds];
  
  for (int offset = 0; offset < numberOfLeds; offset++) {
    ledIndex = (-1 * numberOfLeds / 2) + offset + timeValue;
    ledIndex += pixelOffset;
    if (ledIndex < 0) {
      ledIndex += LED_COUNT;
    } else if (ledIndex >= LED_COUNT) {
      ledIndex -= LED_COUNT;
    }
    if (inInversionMode()) {
      clockColors[ledIndex] = strip.ColorHSV(0x0000, 0x00, 0x00);
    } else if (clockColors[ledIndex] > 0) {
      clockColors[ledIndex] = strip.ColorHSV(0x0000, 0x00, brightness); 
    } else {
      clockColors[ledIndex] = strip.ColorHSV(color, 0xFF, brightness); 
    }
  }
}

/**
 * Get the 'Red' component of a 32-bit color
 */
uint8_t Red(uint32_t color) {
  return (color >> 16) & 0xFF;
}

/**
 * Get the 'Green' component of a 32-bit color
 */
uint8_t Green(uint32_t color) {
  return (color >> 8) & 0xFF;
}

/**
 * Get the 'Blue' component of a 32-bit color
 */
uint8_t Blue(uint32_t color) {
  return color & 0xFF;
}

void initializeColors() {
  EEPROM.write(0, 0x00);  //Set hours hue
  EEPROM.write(1, 0x00);  //Set hours hue
  EEPROM.write(2, 0x55);  //Set minutes hue
  EEPROM.write(3, 0x55);  //Set minutes hue
  EEPROM.write(4, 0xAA);  //Set seconds hue
  EEPROM.write(5, 0xAA);  //Set seconds hue
  EEPROM.write(6, 0x01);  //Set DST
  EEPROM.commit();
  hourColor = (EEPROM.read(0) << 8) | EEPROM.read(1);
  minuteColor = (EEPROM.read(2) << 8) | EEPROM.read(3);
  secondColor = (EEPROM.read(4) << 8) | EEPROM.read(5);
}

/**
 * Adjust the time according to daylight savings time
 */
int daylightSavingsTimeAdjustment() {
  //Variables
  time_t date = timeClient.getEpochTime();

  //Determine if date is in DST
  if (month(date) == 3) {
    if (day((date) >= 8) && (day(date) <= 14)) {
      if (weekday(date) == 1) {
        if (hour(date) == 2) {
          if (!inDaylightSavingsTime) {
            inDaylightSavingsTime = true;
            EEPROM.write(6, 0x01);
          }
        }
      } 
    }
  } else if (month(date) == 11) {
    if (day((date) >= 1) && (day(date) <= 7)) {
      if (weekday(date) == 1) {
        if (hour(date) == 2) {
          if (inDaylightSavingsTime) {
            inDaylightSavingsTime = false;
            EEPROM.write(6, 0x00);
          }
        }
      } 
    }
  }
  return inDaylightSavingsTime ? 1 : 0;
}
