# equinox-clock
Arduino code for a Wi-Fi connected clock.

![Main](https://github.com/ajschwieterman/equinox-clock/blob/master/main.jpeg)

# Hardware
- [Adafruit Feather HUZZAH ESP8266](https://www.adafruit.com/product/2821)
- [Adafruit NeoPixels](https://www.adafruit.com/product/1506)
- [Photo cell resistor](https://www.adafruit.com/product/161)
- [Button switches](https://www.adafruit.com/product/1009)

# Schematic
Below is the wiring that I used to supply 5V of power to the NeoPixels and controller.  I merged a USB-A male connector and micro-USB male connector and added an in-line capacitor.
![Main](https://github.com/ajschwieterman/equinox-clock/blob/master/power.png)

Below is the circuit diagram for the buttons, light sensor, and NeoPixels.
![Main](https://github.com/ajschwieterman/equinox-clock/blob/master/schematic.png)

# Required board libraries
- [esp8266](https://github.com/esp8266/Arduino) v3.0.2
  - Adafruit Feather HUZZAH ESP8266

Use the following instructions to install the Adafruit board libraries and configure the ESP8266 hardware.
- [Board Libraries Installation](https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/using-arduino-ide)
- [Hardware Configuration](https://github.com/Mixiaoxiao/Arduino-HomeKit-ESP8266#recommended-settings-in-ide)

# Required Arduino libraries
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) v1.10.7
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) v6.19.4
- [BobaBlox](https://github.com/robertgallup/BobaBlox) v2.0.1
- [HomeKit-ESP8266](https://github.com/Mixiaoxiao/Arduino-HomeKit-ESP8266) v1.2.0
- [NTPClient](https://github.com/arduino-libraries/NTPClient) v3.2.1
- [Time](https://playground.arduino.cc/Code/Time/) v1.6.1
- [Timer](https://github.com/sstaub/Timer) v1.2.1
