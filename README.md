# Arduino Impulse Meter

## A digital meter for impulse signal style gas or water meters

- Tested with Arduino MEGA and UNO (UNO only fixed IP possible due to code size) 
- Tested with ENC28J60

Using a simple impulse signal from a gas or water meter to increment a counter and transmitting the reading via MQTT.

- Impulse input signaling form a reed sensor or so into an Arduino interrupt input
- Set initial meter reading (once) via MQTT
- EEPROM backed last meter reading (internal EEPROM) 

