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
#include <Ethernet.h>    //<EthernetENC.h> for ENC28J60 //<Ethernet.h>  for W5500

// Enable MqttClient logs
#define MQTT_LOG_ENABLED 0
// Include library
#include <MqttClient.h>

/******************** Ethernet Client Setup *******************/
byte mac[] =      {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xF6};         //must be unique   // see list of used addresses
byte ip[] =       {192, 168, 1, 246 };                          //must be unique
byte gateway[] =  {192, 168, 1, 1 };    
byte subnet[] =   {255, 255, 255, 0 };  


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

#define HW_UART_SPEED								115200L

/************** MQTT DEVICE LOCATION ***************/
#define MQTT_LOC                    "Technik"
#define MQTT_ID										  "Gaszaehler"
const char* MQTT_TOPIC =            "/" MQTT_LOC "/" MQTT_ID ;
const char* MQTT_TOPIC_G =          "/" MQTT_LOC "/" MQTT_ID "/G" ;
const char* MQTT_TOPIC_SET =        "/" MQTT_LOC "/" MQTT_ID "/Set" ;
const char* MQTT_TOPIC_GET =        "/" MQTT_LOC "/" MQTT_ID "/Get" ;

/***************************************************/

MqttClient *mqtt = NULL;
EthernetClient network;

//--------------- OWN STUFF -------------------
// #define       sLED        A0
// unsigned long ledTmr      = 0;
// unsigned char ledState    = 0;

#define       PIN_SENSOR    2            //interupt capable pin

unsigned long zaehler     = 0;
unsigned long currentTime = 0;      
unsigned long sendTmr     = 0;

uint16_t LOC_CNT =   0x20; //unsigned long, 4 bytes of EEPROM
//--------------- OWN STUFF END ---------------

// ============== Object to supply system functions =============
class System: public MqttClient::System {
public:
	unsigned long millis() const {
		return ::millis();
	}
};


// ============== Setup all objects ==============================
void setup() {
  // Setup hardware serial for logging
  Serial.begin(HW_UART_SPEED);
	while (!Serial);
  Serial.print("Boot");

  //-------------- OWN STUFF-----------------
  //pinMode(sLED, OUTPUT);

  pinMode(PIN_SENSOR, INPUT_PULLUP);
  delay(5);
  attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), ISRsensor, FALLING);   //INT 0

  //restore last know value from EEPROM
  readFromEEPROM();
  
  Serial.print(".");
  //------------ OWN STUFF END -----------------

  Ethernet.init(10);        //CS pin of the Ethernet Chip
  Ethernet.begin(mac, ip, gateway, gateway, subnet);  //MAC address for the Arduino
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
  Serial.println("ok");
}

// ============== Main loop ======================
void loop() {
  currentTime = millis();

	// Check connection status //runs once at startup or if connection lost
	if (!mqtt->isConnected()) {
    Serial.print("Connect");
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
    options.password.cstring = (char*)"h0m3455mqtt";
    options.MQTTVersion = 4;
    options.clientID.cstring = (char*)MQTT_ID;
    options.cleansession = true;
    options.keepAliveInterval = 15; // 15 seconds
    MqttClient::Error::type rc = mqtt->connect(options, connectResult);
    if (rc != MqttClient::Error::SUCCESS) {
      //LOG_PRINTFLN("Connection error: %i", rc);
      Serial.println("CON ERR");
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
      
      Serial.print(".");    
      // Add subscribe here if need  //runs once after succesful connection
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

    Serial.println("ok");	
    }	
	}   
  else  // MQQT connection good, normal loop stuff here
  {
    currentTime = millis();
    //Blink status LED
    // if(ledTmr + 500 <= currentTime) {
    //   ledTmr = currentTime;
    //   ledState ^= 1;
    //   digitalWrite(sLED, ledState);
    // }
    
    //send update 1min after last pulse detected (ISRsensor call)
    if((sendTmr >= 1) && (sendTmr + 60000 <= currentTime)) {
      sendTmr = 0;      //send only once        
      
      String m3 = String((float)zaehler/100.0f,2);

      Serial.print(m3); Serial.println("m3");
      publishValue(m3);
    } else if(sendTmr > currentTime) sendTmr = currentTime;  //currentTime overflowed       

		// Idle 
		mqtt->yield(500L);
	}
}  //end loop()

//interrupt routine
void ISRsensor() {
  unsigned long time = millis();
  if((sendTmr + 500 <= time) || (sendTmr > time)) {    // debounce || millis() overflowed
    sendTmr = time;
    zaehler++;  
    saveToEEPROM();   //save to EEPROM  
  }
}

// =============== Publish ==============================================
//Pusblish the current gas meter count
void publishValue(String temp) {
  const char* buf = temp.c_str();
  MqttClient::Message message;
  message.qos = MqttClient::QOS0;
  message.retained = false;
  message.dup = false;
  message.payload = (void*) buf;
  message.payloadLen = strlen(buf);
  mqtt->publish(MQTT_TOPIC_G, message);
}


// ============== Subscription callback ========================================
//Set the counter initial value  via mqtt
void processMessageSet(MqttClient::MessageData& md) {
	const MqttClient::Message& msg = md.message;
	char payload[msg.payloadLen + 1];
	memcpy(payload, msg.payload, msg.payloadLen);
  payload[msg.payloadLen] = '\0';

 	// LOG_PRINTFLN(
	// 	"Message arrived: qos %d, retained %d, dup %d, packetid %d, payload:[%s]",
	// 	msg.qos, msg.retained, msg.dup, msg.id, payload
	// );

  unsigned long temp = strtoul(payload,NULL,10);
  if(temp >= 4819200) {
    zaehler = temp;                         //minimun acceptable m3/100
    saveToEEPROM();
    Serial.print("Set: "); Serial.println(zaehler);
  }
}


//Answer the get request 
void processMessageGet(MqttClient::MessageData& md) {
  //payload doesn't matter, publish anyway
  float m3 = (float)zaehler/100.0f;
  String temp = String(m3, 2);   //2 Nachkommastellen
  publishValue(temp);  
}


//---------------- EEPROM ------------------------
void readFromEEPROM() {
  EEPROM_readAnything(LOC_CNT, zaehler);
}

void saveToEEPROM() {
  EEPROM_writeAnything(LOC_CNT, zaehler);
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