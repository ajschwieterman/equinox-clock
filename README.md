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

# Required Arduino libraries
- Adafruit NeoPixel v1.5.3
- ArduinoJson v6.15.1
- NTPClient v3.2.0
- Time v1.6.0
