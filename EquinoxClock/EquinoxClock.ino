#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Button.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <LightDependentResistor.h>
#include <Timer.h>
#include <TZ.h>

#define BUTTON_PIN                                12
#define CHECKSUM_EEPROM_ADDRESS                   EEPROM_BASE_ADDRESS + 0x06
#define CYCLE_TIME_MS                             50
#define EEPROM_BASE_ADDRESS                       0
#define EEPROM_DEFAULT                            0xFF
#define EEPROM_SIZE                               7
#define HOUR_CHUNK_SIZE                           8
#define HOUR_COLOR_DEFAULT                        NEOPIXEL_COLOR_RED
#define HOUR_COLOR_HIGH_BYTE_EEPROM_ADDRESS       EEPROM_BASE_ADDRESS + 0x00
#define HOUR_COLOR_LOW_BYTE_EEPROM_ADDRESS        EEPROM_BASE_ADDRESS + 0x01
#define INITIALIZATION_DELAY_MS                   500
#define INITIALIZATION_ERROR_FLASH_COLOR          NEOPIXEL_COLOR_RED
#define INITIALIZATION_ERROR_FLASH_FREQUENCY_HZ   0.5
#define INITIALIZATION_TIMEOUT_MS                 30000
#define MIN_PER_HOUR                              ((time_t)60)
#define MINUTE_CHUNK_SIZE                         4
#define MINUTE_COLOR_DEFAULT                      NEOPIXEL_COLOR_GREEN
#define MINUTE_COLOR_HIGH_BYTE_EEPROM_ADDRESS     EEPROM_BASE_ADDRESS + 0x02
#define MINUTE_COLOR_LOW_BYTE_EEPROM_ADDRESS      EEPROM_BASE_ADDRESS + 0x03
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
#define PHOTOCELL_KIND                            LightDependentResistor::GL5528
#define PHOTOCELL_MINIMUM_BRIGHTNESS              5
#define PHOTOCELL_PIN                             A0
#define PHOTOCELL_RESISTOR                        470
#define SECOND_CHUNK_SIZE                         2
#define SECOND_COLOR_DEFAULT                      NEOPIXEL_COLOR_BLUE
#define SECOND_COLOR_HIGH_BYTE_EEPROM_ADDRESS     EEPROM_BASE_ADDRESS + 0x04
#define SECOND_COLOR_LOW_BYTE_EEPROM_ADDRESS      EEPROM_BASE_ADDRESS + 0x05
#define SERIAL_MONITOR_BAUD_RATE                  115200
#define UTC_POOL_SERVER_NAME                      "pool.ntp.org"
#define WIFI_SSID                                 ""
#define WIFI_PASSWORD                             ""

// #if ((NEOPIXEL_COUNT % MIN_PER_HOUR) != 0)
// #error "NEOPIXEL_COUNT must be divisible by 60!"
// #endif

/** Define hardware */
Button button(BUTTON_PIN);
Adafruit_NeoPixel neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
LightDependentResistor photocell(PHOTOCELL_PIN, PHOTOCELL_RESISTOR, PHOTOCELL_KIND);

/* Define modes */
enum Mode {
  INITIALIZE,
  NORMAL
};

/* Other variables */
uint8_t bluePigment;
int brightness;
uint32_t clockColors[NEOPIXEL_COUNT];
int currentTimeMilliseconds;
time_t epochTime;
int fadingIndex;
Timer flashTimer(MILLIS);
uint8_t greenPigment;
uint16_t hourColor;
Timer initializationTimer(MILLIS);
bool ledsOn = true;
uint16_t minuteColor;
Mode mode;
int neopixelIndexHour;
int neopixelIndexMinute;
int neopixelIndexSecond;
uint32_t newColor;
uint32_t previousClockColors[NEOPIXEL_COUNT];
uint32_t previousColor;
time_t previousEpochTime;
Mode previousMode;
unsigned long previousSystemTime;
unsigned long previousClockModeTime;
uint8_t redPigment;
bool savePreviousClockColors;
uint16_t secondColor;
unsigned long systemTime;
tm * timeInfo;

//------------------------------------------------------------------------------------------------------------------------

void setup() {
  /* Start the serial monitor */
  // Serial.begin(SERIAL_MONITOR_BAUD_RATE);
  /* Start the EEPROM service */
  setupEEPROM();
  /* Start the button service */
  button.begin();
  /* Start the NeoPixel service */
  neopixels.begin();
  neopixels.clear();
  neopixels.show();
  /* Start the photocell service */
  photocell.setPhotocellPositionOnGround(false);
  /* Start the Wi-Fi service */
  WiFi.disconnect();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
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
  brightness = max(PHOTOCELL_MINIMUM_BRIGHTNESS, (int)neopixels.gamma8(mapf(photocell.getCurrentRawAnalogValue(), 0, 1024, 0, 255)));
  /* Toggle the clock LEDs if the button was pressed */
  if (button.released()) {
    switch (mode) {
      case NORMAL:
        ledsOn = !ledsOn;
        break;
    }
  }
  /* Perform any setup and cleanup actions when the mode changes */
  if (mode != previousMode) {
    switch (previousMode) {
      case INITIALIZE:
        flashTimer.stop();
        break;
    }
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
  previousEpochTime = epochTime;
  time(&epochTime);
  /* Save the color information of the hands every hour */
  if (hour(&epochTime) != hour(&previousEpochTime)) {
    writeEEPROM();
  }
  /* Change the clock hands' colors every second */
  if (second(&epochTime) != second(&previousEpochTime)) {
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
  neopixelIndexHour = ((hour(&epochTime) % 12) * 5 * NEOPIXEL_PER_MIN) + (minute(&epochTime) * NEOPIXEL_PER_MIN / 12);
  neopixelIndexMinute = (minute(&epochTime) * NEOPIXEL_PER_MIN) + (second(&epochTime) * NEOPIXEL_PER_MIN / MIN_PER_HOUR);
  neopixelIndexSecond = (second(&epochTime) * NEOPIXEL_PER_MIN) + (currentTimeMilliseconds / (MS_PER_SEC / NEOPIXEL_PER_MIN));
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
  if (ledsOn) {
    for (int neopixelIndex = 0; neopixelIndex < NEOPIXEL_COUNT; neopixelIndex++) {
      previousColor = previousClockColors[neopixelIndex];
      newColor = clockColors[neopixelIndex];
      redPigment = fade(red(previousColor), red(newColor), fadingIndex, NEOPIXEL_FADING_TIME_MS);
      greenPigment = fade(green(previousColor), green(newColor), fadingIndex, NEOPIXEL_FADING_TIME_MS);
      bluePigment = fade(blue(previousColor), blue(newColor), fadingIndex, NEOPIXEL_FADING_TIME_MS);
      neopixels.setPixelColor(neopixelIndex, neopixels.Color(redPigment, greenPigment, bluePigment));
    }
  }
  if((systemTime - previousClockModeTime) >= CYCLE_TIME_MS) {
    previousClockModeTime = systemTime;
    neopixels.show();
  }
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
    setupNtpServer();
    mode = NORMAL;
  /* Continue to wait until either a Wi-Fi connection has been established or the initialization times out */
  } else {
    delay(INITIALIZATION_DELAY_MS);
  }
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
      checksum += EEPROM.read(EEPROM_BASE_ADDRESS + eepromAddress);
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
  /* Set the color of all the pixels */
  neopixels.clear();
  neopixels.fill(color, 0, NEOPIXEL_COUNT);
  neopixels.show();
  /* Wait until the cycle time has elapsed */
  delay(abs(CYCLE_TIME_MS - (int)(millis() - systemTime)));
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
 * Convert the epoch time into an object that formats the time into readable language
 * 
 * @param time The epoch time to convert
 * @return tm* Pointer to the timeInfo object
 */
tm * getTimeInfo(time_t * time) {
  timeInfo = localtime(time);
  return timeInfo;
}

/**
 * Get the 'Green' component of a 32-bit color
 */
uint8_t green(uint32_t color) {
  return (color >> 8) & 0xFF;
}

/**
 * Get the 'hour' component of an epoch time
 * 
 * @param time Pointer to the epoch time
 * @return int The 'hour' in the timestamp
 */
int hour(time_t * time) {
  return getTimeInfo(time)->tm_hour;
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
 * Get the 'minute' component of an epoch time
 * 
 * @param time Pointer to the epoch time
 * @return int The 'minute' in the timestamp
 */
int minute(time_t * time) {
  return getTimeInfo(time)->tm_min;
}

/**
 * Get the 'Red' component of a 32-bit color
 */
uint8_t red(uint32_t color) {
  return (color >> 16) & 0xFF;
}

/**
 * Get the 'second' component of an epoch time
 * 
 * @param time Pointer to the epoch time
 * @return int The 'second' in the timestamp
 */
int second(time_t * time) {
  return getTimeInfo(time)->tm_sec;
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
  EEPROM.begin(EEPROM_BASE_ADDRESS + EEPROM_SIZE);
  /* Write default values to EEPROM if the data is not valid */
  if (EEPROM.read(CHECKSUM_EEPROM_ADDRESS) != calculateChecksum()) {
    hourColor = HOUR_COLOR_DEFAULT;
    minuteColor = MINUTE_COLOR_DEFAULT;
    secondColor = SECOND_COLOR_DEFAULT;
    writeEEPROM();
  /* Load hand colors and DST information from EEPROM if the data is valid */
  } else {
    hourColor = (EEPROM.read(HOUR_COLOR_HIGH_BYTE_EEPROM_ADDRESS) << 8) | EEPROM.read(HOUR_COLOR_LOW_BYTE_EEPROM_ADDRESS);
    minuteColor = (EEPROM.read(MINUTE_COLOR_HIGH_BYTE_EEPROM_ADDRESS) << 8) | EEPROM.read(MINUTE_COLOR_LOW_BYTE_EEPROM_ADDRESS);
    secondColor = (EEPROM.read(SECOND_COLOR_HIGH_BYTE_EEPROM_ADDRESS) << 8) | EEPROM.read(SECOND_COLOR_LOW_BYTE_EEPROM_ADDRESS);
  }
}

/**
 * After connecting to Wi-Fi, setup the NTP server connection to fetch the local time.
 */
void setupNtpServer() {
  /* Configure the NTP server connectio */
  configTime(TZ_America_Detroit, UTC_POOL_SERVER_NAME);
  /* Wait until the first valid timestamp has been received */
  while (!time(nullptr)) {
    delay(1000);
  }
  delay(1000);
  /* Save the first valid timestamp */
  time(&epochTime);
  previousEpochTime = epochTime;
}

/**
 * Change the frequency at which the time is fetched from the NTP server.  By default, the time is fetched every 
 * hour, but this function changes the update frequency to every 24 hours.
 */
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000() {
  return 86400000;
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
  EEPROM.commit();
  /* Write the checksum value of the configuration values to EEPROM */
  EEPROM.write(CHECKSUM_EEPROM_ADDRESS, calculateChecksum());
  EEPROM.commit();
}
