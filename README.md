# Arduino Impulse Utility Meter

## A digital meter for impulse signal style gas or water meters

Using a simple impulse signal from a gas or water meter to increment a counter and transmitting the reading via MQTT.

- Impulse signal form a reed or hall effect sensor into an Arduino interrupt input
- Send meter reading updates every min
- Set initial meter reading (once) via MQTT
- EEPROM backed last meter reading (internal EEPROM)

## Current state
- Tested only with Arduino MEGA and ProMini (ProMini only fixed IP possible due to code size) 
- ProMini uses atmega328, so UNO should work too
- Tested only with ENC28J60

- MEGA:     Sketch uses 30778 bytes (12%) of program storage space. Global variables use 1297 bytes (15%) of dynamic memory.
- ProMini:  Sketch uses 29170 bytes (94%) of program storage space. Global variables use 1285 bytes (62%) of dynamic memory.
