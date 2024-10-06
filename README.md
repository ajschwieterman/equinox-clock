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
- [esp8266](https://github.com/esp8266/Arduino) v3.1.2
  - Adafruit Feather HUZZAH ESP8266

Use the following instructions to install the Adafruit board libraries and configure the ESP8266 hardware.
- [Board Libraries Installation](https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/using-arduino-ide)

# Required Arduino libraries
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) v1.12.3
- [Button](https://github.com/madleech/Button) v1.0.0
- [LightDependentResistor](https://github.com/QuentinCG/Arduino-Light-Dependent-Resistor-Library) v1.4.0
- [NeoPixelBus](https://github.com/Makuna/NeoPixelBus) v2.8.3
- [Timer](https://github.com/sstaub/Timer) v1.2.1
