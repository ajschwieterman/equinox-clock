#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <BobaBlox.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Timer.h>
#include <WiFiUdp.h>

#define BUTTON_PIN                                12
#define CHECKSUM_EEPROM_ADDRESS                   0x07
#define DAYLIGHT_SAVINGS_TIME_EEPROM_ADDRESS      0x06
#define DAYLIGHT_SAVINGS_TIME_OFF                 0x00
#define DAYLIGHT_SAVINGS_TIME_ON                  0x01
#define DEBOUNCE_INTERVAL                         50
#define EEPROM_DEFAULT                            0xFF
#define EEPROM_SIZE                               8
#define HOUR_CHUNK_SIZE                           8
#define HOUR_COLOR_DEFAULT                        NEOPIXEL_COLOR_RED
#define HOUR_COLOR_HIGH_BYTE_EEPROM_ADDRESS       0x00
#define HOUR_COLOR_LOW_BYTE_EEPROM_ADDRESS        0x01
#define INITIALIZATION_DELAY_MS                   500
#define INITIALIZATION_ERROR_FLASH_COLOR          NEOPIXEL_COLOR_RED
#define INITIALIZATION_ERROR_FLASH_FREQUENCY_HZ   0.5
#define INITIALIZATION_TIMEOUT_MS                 30000
#define MIN_PER_HOUR                              ((time_t)(SECS_PER_HOUR / SECS_PER_MIN))
#define MINUTE_CHUNK_SIZE                         4
#define MINUTE_COLOR_DEFAULT                      NEOPIXEL_COLOR_GREEN
#define MINUTE_COLOR_HIGH_BYTE_EEPROM_ADDRESS     0x02
#define MINUTE_COLOR_LOW_BYTE_EEPROM_ADDRESS      0x03
#define MS_PER_SEC                                1000
#define NEOPIXEL_COLOR_BLUE                       0xAAAA
#define NEOPIXEL_COLOR_GREEN                      0x5555
#define NEOPIXEL_COLOR_ORANGE                     0x1555
#define NEOPIXEL_COLOR_RED                        0x0000
#define NEOPIXEL_COLOR_WHITE                      0x0000
#define NEOPIXEL_COUNT                            240
#define NEOPIXEL_FADING_TIME_MS                   (MS_PER_SEC / NEOPIXEL_PER_MIN)
#define NEOPIXEL_OFFSET                           118
#define NEOPIXEL_PIN                              14
#define NEOPIXEL_PER_MIN                          (NEOPIXEL_COUNT / MIN_PER_HOUR)
#define NEOPIXEL_SATURATION_OFF                   0x00
#define NEOPIXEL_SATURATION_ON                    0xFF
#define NEOPIXEL_VALUE_OFF                        0x00
#define NOTIFICATION_FLASH_COLOR                  NEOPIXEL_COLOR_ORANGE
#define NOTIFICATION_FLASH_FREQUENCY_HZ           0.5
#define NOTIFICATION_TIMEOUT_MS                   10000
#define PHOTOCELL_MINIMUM_BRIGHTNESS              5
#define PHOTOCELL_PIN                             A0
#define PROGRAM_END_COLOR                         NEOPIXEL_COLOR_GREEN
#define PROGRAM_ERROR_COLOR                       NEOPIXEL_COLOR_RED
#define PROGRAM_FLASH_COLOR                       NEOPIXEL_COLOR_BLUE
#define PROGRAM_FLASH_FREQUENCY_HZ                0.5
#define PROGRAM_PROGRESS_COLOR                    NEOPIXEL_COLOR_ORANGE
#define SECOND_CHUNK_SIZE                         2
#define SECOND_COLOR_DEFAULT                      NEOPIXEL_COLOR_BLUE
#define SECOND_COLOR_HIGH_BYTE_EEPROM_ADDRESS     0x04
#define SECOND_COLOR_LOW_BYTE_EEPROM_ADDRESS      0x05
#define SERIAL_MONITOR_BAUD_RATE                  115200
#define UTC_POOL_SERVER_NAME                      "pool.ntp.org"
#define UTC_TIME_OFFSET_SECONDS                   -18000  /* For UTC -5.00 : -5 * 60 * 60 */
#define UTC_UPDATE_INTERVAL_MS                    ((time_t)(SECS_PER_DAY * MS_PER_SEC))
#define WIFI_SSID                                 ""
#define WIFI_PASSWORD                             ""

// #if ((NEOPIXEL_COUNT % MIN_PER_HOUR) != 0)
// #error "NEOPIXEL_COUNT must be divisible by 60!"
// #endif

/** Define hardware */
Button button(BUTTON_PIN);
Adafruit_NeoPixel neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
WiFiUDP ntpUDP;
Photocell photocell(PHOTOCELL_PIN);
NTPClient timeClient(ntpUDP, UTC_POOL_SERVER_NAME, UTC_TIME_OFFSET_SECONDS, UTC_UPDATE_INTERVAL_MS);
ESP8266WebServer webServer;

/* Define modes */
enum Mode {
  INITIALIZE,
  NORMAL,
  NOTIFICATION,
  PROGRAM
};

/* Other variables */
uint8_t bluePigment;
int brightness;
uint32_t clockColors[NEOPIXEL_COUNT];
int currentTimeMilliseconds;
unsigned long epochTime;
int fadingIndex;
Timer flashTimer(MILLIS);
uint8_t greenPigment;
uint16_t hourColor;
bool inDaylightSavingsTime;
Timer initializationTimer(MILLIS);
uint16_t minuteColor;
Mode mode;
int neopixelIndexHour;
int neopixelIndexMinute;
int neopixelIndexSecond;
uint32_t newColor;
uint16_t notificationColor = NOTIFICATION_FLASH_COLOR;
double notificationFrequency = NOTIFICATION_FLASH_FREQUENCY_HZ;
unsigned long notificationTimeout = NOTIFICATION_TIMEOUT_MS;
Timer notificationTimer(MILLIS);
uint32_t previousClockColors[NEOPIXEL_COUNT];
uint32_t previousColor;
unsigned long previousEpochTime;
Mode previousMode;
unsigned long previousSystemTime;
uint8_t redPigment;
bool savePreviousClockColors;
uint16_t secondColor;
unsigned long systemTime;
StaticJsonDocument<500> webServerDoc;
String webServerJson;
String webServerMessage;

//------------------------------------------------------------------------------------------------------------------------

void setup() {
  /* Start the serial monitor */
  // Serial.begin(SERIAL_MONITOR_BAUD_RATE);
  /* Start the EEPROM service */
  setupEEPROM();
  /* Start the NeoPixel service */
  neopixels.begin();
  neopixels.clear();
  neopixels.show();
  /* Start the Wi-Fi service */
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  /* Start in initialize mode */
  mode = INITIALIZE;
  previousMode = INITIALIZE;
}

void loop() {
  /* Get the system time */
  previousSystemTime = systemTime;
  systemTime = millis();
  /* Set the brightness level of the neopixels based on the ambient light */
  brightness = max(PHOTOCELL_MINIMUM_BRIGHTNESS, (int)neopixels.gamma8(photocell.value(0, 255)));
  /* Monitor for any HTTP requests */
  webServer.handleClient();
  /* Change the clock mode if the button was pressed */
  if (button.wasPressed()) {
    flashTimer.stop();
    switch (mode) {
      case NORMAL:
        mode = PROGRAM;
        break;
      case PROGRAM:
        mode = NORMAL;
        break;
    }
  }
  /* Perform any setup and cleanup actions when the mode changes */
  if (mode != previousMode) {
    switch (mode) {
      case PROGRAM:
        ArduinoOTA.begin();
        break;
    }
    // switch (previousMode) {
    //   case PROGRAM:
    //     ArduinoOTA.end();
    //     break;
    // }
    previousMode = mode;
  }
  /* Switch between the various modes */
  switch (mode) {
    case INITIALIZE:
      initializeMode();
      break;
    case NORMAL:
      clockMode();
      break;
    case PROGRAM:
      programMode();
      break;
    case NOTIFICATION:
      notificationMode();
      break;
  }
}

//------------------------------------------------------------------------------------------------------------------------

/**
 * Normal and inverse modes are the main modes of the program. The current time is fetched via Wi-Fi (if available) or 
 * locally (if Wi-Fi connection was lost), and the clock's "hands" that display the time are represented with chunks 
 * of NeoPixels. Daylight savings time is tracked and accounted for while operating. Occassionally, the colors of the 
 * "hands" are stored to be restored in the event of a power cycle.
 */
void clockMode() {
  /* Fetch the time from the NTP server */
  timeClient.update();
  previousEpochTime = epochTime;
  epochTime = timeClient.getEpochTime() + daylightSavingsTime();
  /* Save the color information of the hands every hour */
  if (hour(epochTime) != hour(previousEpochTime)) {
    writeEEPROM();
  }
  /* Change the clock hands' colors every second */
  if (second(epochTime) != second(previousEpochTime)) {
    hourColor = (hourColor + 1) & 0xFFFF;
    minuteColor = (minuteColor + 1) & 0xFFFF;
    secondColor = (secondColor + 1) & 0xFFFF;
    currentTimeMilliseconds = 0;
  /* Calculate the amount of milliseconds that have passed within the last second */
  } else {
    currentTimeMilliseconds = min((int)(currentTimeMilliseconds + systemTime - previousSystemTime), 999);
  }
  fadingIndex = currentTimeMilliseconds % NEOPIXEL_FADING_TIME_MS;
  /* Save the previous clock colors if a fading sequence has completed */
  if (fadingIndex < (NEOPIXEL_FADING_TIME_MS / 2) && savePreviousClockColors) {
    for (int neopixelIndex = 0; neopixelIndex < NEOPIXEL_COUNT; neopixelIndex++) {
      previousClockColors[neopixelIndex] = clockColors[neopixelIndex];
    }
    savePreviousClockColors = false;
  } else if (fadingIndex > (NEOPIXEL_FADING_TIME_MS / 2) && !savePreviousClockColors) {
    savePreviousClockColors = true;
  }
  /* Get the neopixel indexes of the hours, minutes, and seconds hands */
  neopixelIndexHour = ((hour(epochTime) % 12) * 5 * NEOPIXEL_PER_MIN) + (minute(epochTime) * NEOPIXEL_PER_MIN / 12);
  neopixelIndexMinute = (minute(epochTime) * NEOPIXEL_PER_MIN) + (second(epochTime) * NEOPIXEL_PER_MIN / MIN_PER_HOUR);
  neopixelIndexSecond = (second(epochTime) * NEOPIXEL_PER_MIN) + (currentTimeMilliseconds / (MS_PER_SEC / NEOPIXEL_PER_MIN));
  /* Prepare the clock's colors for new values */
  for (int index = 0; index < NEOPIXEL_COUNT; index++) {
    clockColors[index] = neopixels.ColorHSV(NEOPIXEL_COLOR_WHITE, NEOPIXEL_SATURATION_OFF, NEOPIXEL_VALUE_OFF);
  }
  /* Set the colors of the clock */
  setClockHandColors(neopixelIndexHour, HOUR_CHUNK_SIZE, hourColor);
  setClockHandColors(neopixelIndexMinute, MINUTE_CHUNK_SIZE, minuteColor);
  setClockHandColors(neopixelIndexSecond, SECOND_CHUNK_SIZE, secondColor);
  /* Apply fading effect to the clock's hands and display the colors of the clock */
  neopixels.clear();
  for (int neopixelIndex = 0; neopixelIndex < NEOPIXEL_COUNT; neopixelIndex++) {
    previousColor = previousClockColors[neopixelIndex];
    newColor = clockColors[neopixelIndex];
    redPigment = fade(red(previousColor), red(newColor), fadingIndex, NEOPIXEL_FADING_TIME_MS);
    greenPigment = fade(green(previousColor), green(newColor), fadingIndex, NEOPIXEL_FADING_TIME_MS);
    bluePigment = fade(blue(previousColor), blue(newColor), fadingIndex, NEOPIXEL_FADING_TIME_MS);
    neopixels.setPixelColor(neopixelIndex, neopixels.Color(redPigment, greenPigment, bluePigment));
  }
  neopixels.show();
}

/**
 * Initialize mode waits until a Wi-Fi connection has been established. If it cannot connect to Wi-Fi, an error color
 * will flash indicating there was an error connecting to Wi-Fi, and the program will stall here until the next power
 * cycle.
 */
void initializeMode() {
  /* Start the initialization timer */
  if ((initializationTimer.state() != RUNNING) && (initializationTimer.read() < INITIALIZATION_TIMEOUT_MS)) {
    initializationTimer.start();
  /* If initialization times out, stop attempting to connnect to Wi-Fi, and flash the error color */
  } else if (initializationTimer.read() >= INITIALIZATION_TIMEOUT_MS) {
    if (initializationTimer.state() != STOPPED) {
      initializationTimer.stop();
      WiFi.disconnect();
    }
    flash(INITIALIZATION_ERROR_FLASH_COLOR, INITIALIZATION_ERROR_FLASH_FREQUENCY_HZ);
  /* Connection to Wi-Fi has been established, so continue with initialization, then to normal operations */
  } else if (WiFi.status() == WL_CONNECTED) {
    initializationTimer.stop();
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    timeClient.begin();
    setupOtaUpdates();
    setupWebServer();
    mode = NORMAL;
  /* Continue to wait until either a Wi-Fi connection has been established or the initialization times out */
  } else {
    delay(INITIALIZATION_DELAY_MS);
  }
}

/**
 * Notification mode will flash a notification color at a frequency and duration specified by the HTTP request 
 * received. After the duration has passed, normal operations will be resumed.
 */
void notificationMode() {
  /* Start the notification timer */
  if (notificationTimer.state() != RUNNING) {
    notificationTimer.start();
  /* Once the notification timer times out, return to normal operations */
  } else if (notificationTimer.read() >= notificationTimeout) {
    notificationTimer.stop();
    mode = NORMAL;
  /* The notification timer has not timed out yet, so continue flashing the notification color */
  } else {
    flash(notificationColor, notificationFrequency);
  }
}

/**
 * Program mode allows the board to be flashed with a new application via over-the-air updates. It will flash the 
 * program color until either a new application has been flashed or program mode has been exited and normal mode has 
 * been resumed via the "mode button" pressed by a user.
 */
void programMode() {
  /* Flash the neopixels to indicate programming mode */
  flash(PROGRAM_FLASH_COLOR, PROGRAM_FLASH_FREQUENCY_HZ);
  /* Handle incoming over-the-air updates */
  ArduinoOTA.handle();
}

//------------------------------------------------------------------------------------------------------------------------

/**
 * Get the 'Blue' component of a 32-bit color
 */
uint8_t blue(uint32_t color) {
  return color & 0xFF;
}

/**
 * Calculate the expected checksum value based on the configuration values stored in EEPROM.
 * 
 * @return The calculated checksum of EEPROM
 */
uint8_t calculateChecksum() {
  uint8_t checksum = 0x00;
  for (int eepromAddress = 0x00; eepromAddress < EEPROM_SIZE; eepromAddress++) {
    if (eepromAddress != CHECKSUM_EEPROM_ADDRESS) {
      checksum += EEPROM.read(eepromAddress);
    }
  }
  return checksum;
}

/**
 * Set all of the NeoPixels to one color.
 * 
 * @param color   The color/hue to set.
 */
void colorWipe(uint32_t color) {
  neopixels.clear();
  for (int neopixelIndex = 0; neopixelIndex < NEOPIXEL_COUNT; neopixelIndex++) {
    neopixels.setPixelColor(neopixelIndex, color);
  }
  neopixels.show();
}

/**
 * Return the amount of seconds to add to the current time if we are in daylight savings time.
 * 
 * @return The amount of seconds to add for daylight savings time
 */
unsigned long daylightSavingsTime() {
  /* Daylight savings time starts on the second Sunday of March at 2am */
  if ((month(epochTime) == 3) && 
      (day(epochTime) >= 8) && (day(epochTime) <= 14) &&
      (weekday(epochTime) == 1) &&
      (hour(epochTime) == 2) &&
      (!inDaylightSavingsTime)) {
        inDaylightSavingsTime = true;
        writeEEPROM();
  }
  /* Daylight savings time ends on the first Sunday of November at 2am */
  if ((month(epochTime) == 11) &&
      (day(epochTime) >= 1) && (day(epochTime) <= 7) &&
      (weekday(epochTime) == 1) &&
      (hour(epochTime) == 2) &&
      (inDaylightSavingsTime)) {
        inDaylightSavingsTime = false;
        writeEEPROM();
  }
  return (inDaylightSavingsTime ? SECS_PER_HOUR : 0);
}

/**
 * Fade two colors into one.
 * 
 * @param previousColor   The previous color to fade from
 * @param newColor        The new color to fade into
 * @param index           The index at which to fade to the new color
 * @param steps           The base which index is referenced
 * @return                The newly faded color
 */
uint8_t fade(int previousColor, int newColor, int index, int steps) {
  return (((previousColor * (steps - index)) + (newColor * index)) / steps) & 0xFF;
}

/**
 * Flash all of the NeoPixels in a sinusoidal pattern.
 * 
 * @param color       The color/hue to flash
 * @param frequency   The rate at which to flash the NeoPixels
 */
void flash(uint16_t color, double frequency) {
  /* Start the flash timer */
  if ((flashTimer.state() != RUNNING) || (flashTimer.read() >= ((1.0 / frequency) * MS_PER_SEC))) {
    flashTimer.start();
  }
  /* Set the color of all the NeoPixels */
  colorWipe(neopixels.ColorHSV(color, NEOPIXEL_SATURATION_ON, mapf(sin(2 * M_PI * frequency * ((double)flashTimer.read() / MS_PER_SEC) - M_PI / 2), -1, 1, 0, 1) * brightness));
}

/**
 * Get the 'Green' component of a 32-bit color
 */
uint8_t green(uint32_t color) {
  return (color >> 8) & 0xFF;
}

/**
 * Re-maps a 'double' number from one range to another.
 * 
 * @param x         The number to map
 * @param in_min    The lower bound of the value's current range
 * @param in_max    The higher bound of the value's current range
 * @param out_min   The lower bound of the value's target range
 * @param out_max   The lower bound of the value's target range
 * @return          The mapped value
 */
double mapf(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * Get the 'Red' component of a 32-bit color
 */
uint8_t red(uint32_t color) {
  return (color >> 16) & 0xFF;
}

/**
 * Set the color and location of a clock hand.
 * 
 * @param index   The original index of where the hand is located
 * @param size    The number of NeoPixels that represent the hand
 * @param color   The color of the hand to display
 */
void setClockHandColors(int index, int size, uint16_t color) {
  int neopixelIndex;
  for (int offset = 0; offset < size; offset++) {
    /* Calculate the neopixel index  */
    neopixelIndex = (-1 * size / 2) + offset + index + NEOPIXEL_OFFSET;
    if (neopixelIndex < 0) {
      neopixelIndex += NEOPIXEL_COUNT;
    } else if (neopixelIndex >= NEOPIXEL_COUNT) {
      neopixelIndex -= NEOPIXEL_COUNT;
    }
    /* Set the color of the neopixel */
    if (clockColors[neopixelIndex] > 0) {
      clockColors[neopixelIndex] = neopixels.ColorHSV(NEOPIXEL_COLOR_WHITE, NEOPIXEL_SATURATION_OFF, brightness);
    } else {
      clockColors[neopixelIndex] = neopixels.ColorHSV(color, NEOPIXEL_SATURATION_ON, brightness);
    }
  }
}

/**
 * Verify the EEPROM is not defaulted before beginning normal operations and load the hand colors and DST information from
 * EEPROM.
 */
void setupEEPROM() {
  /* Start the EEPROM service */
  EEPROM.begin(EEPROM_SIZE);
  /* Write default values to EEPROM if the data is not valid */
  if (EEPROM.read(CHECKSUM_EEPROM_ADDRESS) != calculateChecksum()) {
    hourColor = HOUR_COLOR_DEFAULT;
    minuteColor = MINUTE_COLOR_DEFAULT;
    secondColor = SECOND_COLOR_DEFAULT;
    inDaylightSavingsTime = true;
    writeEEPROM();
  /* Load hand colors and DST information from EEPROM if the data is valid */
  } else {
    hourColor = (EEPROM.read(HOUR_COLOR_HIGH_BYTE_EEPROM_ADDRESS) << 8) | EEPROM.read(HOUR_COLOR_LOW_BYTE_EEPROM_ADDRESS);
    minuteColor = (EEPROM.read(MINUTE_COLOR_HIGH_BYTE_EEPROM_ADDRESS) << 8) | EEPROM.read(MINUTE_COLOR_LOW_BYTE_EEPROM_ADDRESS);
    secondColor = (EEPROM.read(SECOND_COLOR_HIGH_BYTE_EEPROM_ADDRESS) << 8) | EEPROM.read(SECOND_COLOR_LOW_BYTE_EEPROM_ADDRESS);
    inDaylightSavingsTime = (EEPROM.read(DAYLIGHT_SAVINGS_TIME_EEPROM_ADDRESS) == DAYLIGHT_SAVINGS_TIME_ON);
  }
}

/**
 * After connecting to Wi-Fi, set up over-the-air updates to reflash the application over Wi-Fi rather than through
 * the USB cable.
 */
void setupOtaUpdates() {
  /* Method to call when programming mode has started */
  ArduinoOTA.onStart([]() {
    colorWipe(neopixels.ColorHSV(NEOPIXEL_COLOR_WHITE, NEOPIXEL_SATURATION_OFF, NEOPIXEL_VALUE_OFF));
  });
  /* Method to call when programming mode has ended */
  ArduinoOTA.onEnd([]() {
    colorWipe(neopixels.ColorHSV(PROGRAM_END_COLOR, NEOPIXEL_SATURATION_ON, brightness));
  });
  /* Method to call while programming */
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    neopixels.clear();
    for (int neopixelIndex = 0; neopixelIndex < NEOPIXEL_COUNT; neopixelIndex++) {
      if (neopixelIndex < map(progress, 0, total, 0, NEOPIXEL_COUNT)) {
        neopixels.setPixelColor(neopixelIndex, neopixels.ColorHSV(PROGRAM_PROGRESS_COLOR, NEOPIXEL_SATURATION_ON, brightness));
      } else {
        neopixels.setPixelColor(neopixelIndex, neopixels.ColorHSV(NEOPIXEL_COLOR_WHITE, NEOPIXEL_SATURATION_OFF, NEOPIXEL_VALUE_OFF));
      }
    }
    neopixels.show();
  });
  /* Method to call when an error occurs when programming */
  ArduinoOTA.onError([](ota_error_t error) {
    colorWipe(neopixels.ColorHSV(PROGRAM_ERROR_COLOR, NEOPIXEL_SATURATION_ON, brightness));
  });
}

/**
 * After connecting to Wi-Fi, set up the web server to receive HTTP requests for tasks.
 */
void setupWebServer() {
  /* Return the ambient brightness when requested */
  webServer.on("/equinox", [](){
    webServerJson = "";
    webServerDoc["brightness"] = brightness;
    serializeJsonPretty(webServerDoc, webServerJson);
    webServer.send(200, "application/json", webServerJson);
  });
  /* Begin normal operations when requested */
  webServer.on("/normal", [](){
    mode = NORMAL;
    webServerMessage = "Normal mode initiated";
    webServer.send(200, "text/plain", webServerMessage);
  });
  /* Begin notification mode when requested with some customization to the flashing and timing parameters */
  webServer.on("/notify", [](){
    mode = NOTIFICATION;
    notificationTimer.stop();
    webServerMessage = "Number of args received:";
    webServerMessage += webServer.args();
    webServerMessage += "\n";
    for (int argsIndex = 0; argsIndex < webServer.args(); argsIndex++) {
      if (webServer.argName(argsIndex) == "color") {
        notificationColor = webServer.arg(argsIndex).toInt();
      } else if (webServer.argName(argsIndex) == "duration") {
        notificationTimeout = webServer.arg(argsIndex).toInt();
      } else if (webServer.argName(argsIndex) == "flash") {
        notificationFrequency = webServer.arg(argsIndex).toInt();
      }
      webServerMessage += "Arg n�" + (String)argsIndex + " �> ";
      webServerMessage += webServer.argName(argsIndex) + ": ";
      webServerMessage += webServer.arg(argsIndex) + "\n";
    }
    webServer.send(200, "text/plain", webServerMessage);
  });
  /* Begin programming mode when requested */
  webServer.on("/program", [](){
    mode = PROGRAM;
    webServerMessage = "Program mode initiated";
    webServer.send(200, "text/plain", webServerMessage);
  });
  /* Start the web server service */
  webServer.begin();
}

/**
 * Write the configuration values to EEPROM along with the checksum to validate the data upon the next initialization.
 */
void writeEEPROM() {
  /* Write all confiugration values to EEPROM */
  EEPROM.write(HOUR_COLOR_HIGH_BYTE_EEPROM_ADDRESS, highByte(hourColor));
  EEPROM.write(HOUR_COLOR_LOW_BYTE_EEPROM_ADDRESS, lowByte(hourColor));
  EEPROM.write(MINUTE_COLOR_HIGH_BYTE_EEPROM_ADDRESS, highByte(minuteColor));
  EEPROM.write(MINUTE_COLOR_LOW_BYTE_EEPROM_ADDRESS, lowByte(minuteColor));
  EEPROM.write(SECOND_COLOR_HIGH_BYTE_EEPROM_ADDRESS, highByte(secondColor));
  EEPROM.write(SECOND_COLOR_LOW_BYTE_EEPROM_ADDRESS, lowByte(secondColor));
  EEPROM.write(DAYLIGHT_SAVINGS_TIME_EEPROM_ADDRESS, inDaylightSavingsTime ? DAYLIGHT_SAVINGS_TIME_ON : DAYLIGHT_SAVINGS_TIME_OFF);
  EEPROM.commit();
  /* Write the checksum value of the configuration values to EEPROM */
  EEPROM.write(CHECKSUM_EEPROM_ADDRESS, calculateChecksum());
  EEPROM.commit();
}
