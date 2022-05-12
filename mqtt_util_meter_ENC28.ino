/************************** MQTT Utility meter by Orbis92 ********************************
 *        MQTT utility meter for impulse signaling with internal EEPROM storage          *
 *                                                                                       *
 *  - Initial counter can be set via MQTT, but beware of what you send!                  *
 *    The raw counter is set, only numbers allowed, check "processMessageSet(..)" first  *
 *  - Check the publishCounter() too, some conversion to the real meter reading is done  *
 *                                                                                       *
 *****************************************************************************************/

// #1 For the main part of the code a big thanks to the following author(s). 
// #2 Thanks for the ENC28J60 lib to replace the W5500 Phy, which is a pain to use, at least for me somehow...
// #3 Thanks for the EEPROM "fix" found on the Arduino Playgound //see end of code

/*
 *******************************************************************************
 *
 * Purpose: Example of using the Arduino MqttClient with EthernetClient.
 * Project URL: https://github.com/monstrenyatko/ArduinoMqtt
 *
 *******************************************************************************
 * Copyright Oleg Kovalenko 2017.
 *
 * Distributed under the MIT License.
 * (See accompanying file LICENSE or copy at http://opensource.org/licenses/MIT)
 *******************************************************************************
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <EthernetENC.h>    //<EthernetENC.h> for ENC28J60   //<Ethernet.h>  for W5500 (not tested)

// Enable MqttClient logs
#define MQTT_LOG_ENABLED 0 //disable log
// Include library
#include <MqttClient.h>

/******************************** Ethernet Client Setup ********************************/
byte mac[] =      {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xF6};         //must be unique
byte ip[] =       {192, 168, 1, 246 };                          //must be unique
byte gateway[] =  {192, 168, 1, 1 };    
byte subnet[] =   {255, 255, 255, 0 };  

/***************************************************************************************/

#define HW_UART_SPEED	      115200L

/***************************** MQTT DEVICE LOCATION *************************************/
#define MQTT_LOC                    "UtilityRoom"
#define MQTT_ID			                "UtilityMeter"
const char* MQTT_TOPIC =            "/" MQTT_LOC "/" MQTT_ID ;
const char* MQTT_TOPIC_COUNT =      "/" MQTT_LOC "/" MQTT_ID "/Count" ;
const char* MQTT_TOPIC_SET =        "/" MQTT_LOC "/" MQTT_ID "/Set" ;
const char* MQTT_TOPIC_GET =        "/" MQTT_LOC "/" MQTT_ID "/Get" ;

/***************************************************************************************/

MqttClient *mqtt = NULL;
EthernetClient network;

//Status Led
// #define       sLED        A0		  //Pin 13 Led is most likely to be SPI Clock
// unsigned long ledTmr      = 0;
// unsigned char ledState    = 0;

//Sensor
#define       PIN_SENSOR    2            //interupt capable pin
unsigned long counter     = 0;

//timers
unsigned long currentTime = 0;      
unsigned long sendTmr     = 0;
unsigned long debounceTmr = 0;

//EEPROM position of the counter backup
uint16_t LOC_CNT =   0x20;


//============== MQTT logging ===================================
// #define LOG_PRINTFLN(fmt, ...)	printfln_P(PSTR(fmt), ##__VA_ARGS__)
// #define LOG_SIZE_MAX 128
// void printfln_P(const char *fmt, ...) {
// 	char buf[LOG_SIZE_MAX];
// 	va_list ap;
// 	va_start(ap, fmt);
// 	vsnprintf_P(buf, LOG_SIZE_MAX, fmt, ap);
// 	va_end(ap);
// 	Serial.println(buf);
// }

// ============== Object to supply system functions =============
class System: public MqttClient::System {
public:
	unsigned long millis() const {
		return ::millis();
	}
};


// ===================================== SETUP =============================================
void setup() {
  // Setup hardware serial for logging
  Serial.begin(HW_UART_SPEED);
	while (!Serial);
  Serial.print("Boot");

  //pinMode(sLED, OUTPUT);  //status led

  //sensor input
  pinMode(PIN_SENSOR, INPUT_PULLUP);
  delay(5);
  attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), ISRsensor, FALLING);   //INT 0

  //restore last know value from EEPROM
  readFromEEPROM();
  
  Serial.print(".");

  // Connect ethernet
  Ethernet.init(10);                                  //CS pin of the Ethernet Chip
  //Ethernet.begin(mac);                              //for DHCP, uses a lot of both RAM and ROM
  Ethernet.begin(mac, ip, gateway, gateway, subnet);  //static ip
  Serial.print(".");

  // Setup MqttClient
  MqttClient::System *mqttSystem = new System;
  MqttClient::Logger *mqttLogger = new MqttClient::LoggerImpl<HardwareSerial>(Serial);
  MqttClient::Network * mqttNetwork = new MqttClient::NetworkClientImpl<Client>(network, *mqttSystem);
  //// Make 128 bytes send buffer
  MqttClient::Buffer *mqttSendBuffer = new MqttClient::ArrayBuffer<64>();
  //// Make 128 bytes receive buffer
  MqttClient::Buffer *mqttRecvBuffer = new MqttClient::ArrayBuffer<64>();
  //// Allow up to 2 subscriptions simultaneously
  MqttClient::MessageHandlers *mqttMessageHandlers = new MqttClient::MessageHandlersImpl<2>();
  //// Configure client options
  MqttClient::Options mqttOptions;
  ////// Set command timeout to 10 seconds
  mqttOptions.commandTimeoutMs = 10000;
  //// Make client object
  mqtt = new MqttClient (
    mqttOptions, *mqttLogger, *mqttSystem, *mqttNetwork, *mqttSendBuffer,
    *mqttRecvBuffer, *mqttMessageHandlers
  );

  Serial.println("ok");   //Boot...ok
}


// ====================================== LOOP =============================================
void loop() {  
	// Check connection status //runs once at startup or if connection lost
	if (!mqtt->isConnected()) {
    Serial.print("Con");
		// Close connection if exists
		network.stop();
		// Re-establish TCP connection with MQTT broker
		network.connect("192.168.1.16", 1883);              //IP of your  MQTT broker
		// Start new MQTT connection
		//LOG_PRINTFLN("Connecting to Homeassistant");
		MqttClient::ConnectResult connectResult;
		// Connect
    Serial.print(".");
    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    options.username.cstring = (char*)"mqtt-user";    //Login  to your MQTT broker
    options.password.cstring = (char*)"password";
    options.MQTTVersion = 4;
    options.clientID.cstring = (char*)MQTT_ID;
    options.cleansession = true;
    options.keepAliveInterval = 15; // 15 seconds
    MqttClient::Error::type rc = mqtt->connect(options, connectResult);
    if (rc != MqttClient::Error::SUCCESS) {
      //LOG_PRINTFLN("Connection error: %i", rc);
      Serial.println("ERR");
      return;
    } else  {
      //Serial.print(".");
      
      //send ONLINE info      
      // const char* buf = "ONLINE";
      // MqttClient::Message message;
      // message.qos = MqttClient::QOS0;
      // message.retained = false;
      // message.dup = false;
      // message.payload = (void*) buf;
      // message.payloadLen = strlen(buf);
      // mqtt->publish(MQTT_TOPIC, message);
      
      // Add subscribe here if need
      Serial.print(".");    
      {
        MqttClient::Error::type rc = mqtt->subscribe(MQTT_TOPIC_SET, MqttClient::QOS0, processMessageSet);
        if (rc != MqttClient::Error::SUCCESS) {
          // LOG_PRINTFLN("Subscribe error: %i", rc);
          // LOG_PRINTFLN("Drop connection");
          Serial.println("SUB ERR");
          mqtt->disconnect();
          return;
        }	
      }
      Serial.print(".");
      {
        MqttClient::Error::type rc = mqtt->subscribe(MQTT_TOPIC_GET, MqttClient::QOS0, processMessageGet);
        if (rc != MqttClient::Error::SUCCESS) {
          // LOG_PRINTFLN("Subscribe error: %i", rc);
          // LOG_PRINTFLN("Drop connection");
          Serial.println("SUB ERR");
          mqtt->disconnect();
          return;
        }	
      }

    Serial.println("ok");	  //Con...ok
    }	
	}   
  else  // MQQT connection good, "normal loop stuff" here
  {
    currentTime = millis();

    //Blink status LED
    // if(ledTmr + 500 <= currentTime) {  //1Hz
    //   ledTmr = currentTime;
    //   ledState ^= 1;
    //   digitalWrite(sLED, ledState);
    // }
    
    //send update every 60s
    if(sendTmr + 60000 <= currentTime) {
      sendTmr = currentTime;                          
      
      publishCounter();
    } else if(sendTmr > currentTime) sendTmr = currentTime;  //millis() overflowed  (after approx 50days)   



		// Idle 500ms
		mqtt->yield(500L);
	}   //end "normal loop stuff" 
}  //end loop()


//=======================- interrupt routine =================================
void ISRsensor() {
  unsigned long time = millis();
  if((debounceTmr + 200 <= time) || (debounceTmr > time)) {    // debounce timer || millis() overflowed (after approx 50days) 
    debounceTmr = time;
    
    counter++;  
    saveToEEPROM();   //save to EEPROM  
  }
}

// =============== Publish MQTT==============================================

//Pusblish the current gas meter count
void publishCounter() {
  //Convert counter value to the actual meter reading (e.g. 0.01m³ resolution for my gas meter)
  String m3 = String((float)counter/100.0f, 2);   //2 decimal places
  //Serial.print(m3); Serial.println("m3");
  
  //Publish message
  const char* buf = m3.c_str();
  MqttClient::Message message;
  message.qos = MqttClient::QOS0;
  message.retained = false;
  message.dup = false;
  message.payload = (void*) buf;
  message.payloadLen = strlen(buf);
  mqtt->publish(MQTT_TOPIC_COUNT, message);
}


// ============== MQTT Subscription callback ========================================

//Set the counter initial value via mqtt
void processMessageSet(MqttClient::MessageData& md) {
	const MqttClient::Message& msg = md.message;
	char payload[msg.payloadLen + 1];
	memcpy(payload, msg.payload, msg.payloadLen);
  payload[msg.payloadLen] = '\0';

 	// LOG_PRINTFLN(
	// 	"Message arrived: qos %d, retained %d, dup %d, packetid %d, payload:[%s]",
	// 	msg.qos, msg.retained, msg.dup, msg.id, payload
	// );

  if(msg.payloadLen >= 7)  {                              //minimum acceptable meter reading, see below
    unsigned long temp = strtoul(payload, NULL, 10);      //decimal string to unsigned long 
    if(temp >= 4819200) {                                 //minimun acceptable for my gas meter is 48192.00m3
      counter = temp;                         
      saveToEEPROM();
      //Serial.print("Set: "); Serial.println(counter);
    }
  }
}

//Answer the get request 
void processMessageGet(MqttClient::MessageData& md) {
  //payload doesn't matter, publish anyway
  
  publishCounter();
}


//================================ EEPROM ==============================================
void readFromEEPROM() {
  EEPROM_readAnything(LOC_CNT, counter);
}

void saveToEEPROM() {
  EEPROM_writeAnything(LOC_CNT, counter);
}

//EEPROM workaround from https://playground.arduino.cc/Code/EEPROMWriteAnything/
template <class T> int EEPROM_writeAnything(int ee, const T& value) {
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.update(ee++, *p++);
  return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value) {
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}