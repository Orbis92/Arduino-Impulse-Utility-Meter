# Arduino Impulse Utility Meter

## A digital meter for impulse signal style gas or water meters

Using a simple impulse signal from a gas or water meter to increment a counter and transmitting the reading via MQTT.

- Impulse signal form a reed or hall effect sensor into an Arduino interrupt input
- Send meter reading updates every min
- Set initial meter reading (once) via MQTT
- EEPROM backed last meter reading (internal EEPROM)

## Current state
- Tested only with Arduino MEGA and UNO (UNO only fixed IP possible due to code size) 
- Tested only with ENC28J60

