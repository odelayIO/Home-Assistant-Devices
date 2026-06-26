//#############################################################################################
//#############################################################################################
//#
//#   The MIT License (MIT)
//#   
//#   Copyright (c) 2026 http://odelay.io 
//#   
//#   Permission is hereby granted, free of charge, to any person obtaining a copy
//#   of this software and associated documentation files (the "Software"), to deal
//#   in the Software without restriction, including without limitation the rights
//#   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//#   copies of the Software, and to permit persons to whom the Software is
//#   furnished to do so, subject to the following conditions:
//#   
//#   The above copyright notice and this permission notice shall be included in all
//#   copies or substantial portions of the Software.
//#   
//#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//#   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//#   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//#   SOFTWARE.
//#   
//#   Contact : <everett@odelay.io>
//#  
//#   Description : This is the controller for the puch rack.  Controls the power switches
//#                 and the workstation fan to keep the NVMe cool.
//#
//#
//#     Set the logging level define below.  E.G.: LOG_LEVEL = LOG_LEVEL_SILENT
//#       * 0 - LOG_LEVEL_SILENT     no output 
//#       * 1 - LOG_LEVEL_FATAL      fatal errors 
//#       * 2 - LOG_LEVEL_ERROR      all errors  
//#       * 3 - LOG_LEVEL_WARNING    errors, and warnings 
//#       * 4 - LOG_LEVEL_NOTICE     errors, warnings and notices 
//#       * 5 - LOG_LEVEL_TRACE      errors, warnings, notices & traces 
//#       * 6 - LOG_LEVEL_VERBOSE    all 
//#
//#
//#   Version History:
//#   
//#       Date        Description
//#     -----------   -----------------------------------------------------------------------
//#      2026-01-25    Original Creation
//#
//###########################################################################################
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "arduino_secrets.h"
#include "ArduinoLog.h"



//########################################################################
//#   Log Level (see note above)
//#   Set to VERBOSE during development, then SILENT for operation
//########################################################################

#define LOG_LEVEL LOG_LEVEL_VERBOSE
//#define LOG_LEVEL LOG_LEVEL_SILENT



//########################################################################
//#   Fan Constants for PWM (need to invert due to NPN Transistor)
//########################################################################
uint8_t FAN_OFF   = 255-0;
uint8_t FAN_LOW   = 255-102;  // 40%
uint8_t FAN_MED   = 255-179;  // 70%
uint8_t FAN_HIGH  = 255-255;

uint8_t FAN_INIT  = FAN_HIGH; // on boot
uint8_t PIN_INIT  = LOW; // on boot

#define PWM_FREQ 25000   // 25 kHz (PC fan spec)
#define PWM_RES  8       // 8-bit (0–255)

//########################################################################
//#   ESP32 Pin Mapping
//########################################################################
uint8_t FAN_PWM_PIN = D0; 
uint8_t PLUTO1_PIN  = D10;
uint8_t PLUTO2_PIN  = D2;
uint8_t KRIA_PIN    = D8;
uint8_t PYNQZ1_PIN  = D7;


//########################################################################
//#   Per-Device MQTT Topics
//#     status  - published by XIAO to report current state
//#     command - subscribed by XIAO to receive HA commands
//########################################################################
const char topicKriaStatus[]    = "puch_rack/kria/status";
const char topicKriaCtrl[]      = "puch_rack/kria/command";
const char topicPynqz1Status[]  = "puch_rack/pynqz1/status";
const char topicPynqz1Ctrl[]    = "puch_rack/pynqz1/command";
const char topicPluto1Status[]  = "puch_rack/pluto1/status";
const char topicPluto1Ctrl[]    = "puch_rack/pluto1/command";
const char topicPluto2Status[]  = "puch_rack/pluto2/status";
const char topicPluto2Ctrl[]    = "puch_rack/pluto2/command";
const char topicFanStatus[]     = "puch_rack/fan/status";
const char topicFanCtrl[]       = "puch_rack/fan/command";
const char topicWifiRssi[]      = "puch_rack/wifi/rssi";

const char* ctrlTopics[] = {
  topicKriaCtrl,
  topicPynqz1Ctrl,
  topicPluto1Ctrl,
  topicPluto2Ctrl,
  topicFanCtrl,
};
const size_t ctrlTopicCount = sizeof(ctrlTopics) / sizeof(ctrlTopics[0]);


//########################################################################
//#   Initial Commands Structure - Status to send on boot
//########################################################################
struct {
  uint8_t espPin;
  bool state;
  bool isAnalog;
  uint8_t analogValue;
  const char* feedback;
  const char* statusTopic;
} initCommands[] = {
  {KRIA_PIN,    0, false, 0,        "OFF",  topicKriaStatus},
  {PYNQZ1_PIN,  0, false, 0,        "OFF",  topicPynqz1Status},
  {PLUTO1_PIN,  0, false, 0,        "OFF",  topicPluto1Status},
  {PLUTO2_PIN,  0, false, 0,        "OFF",  topicPluto2Status},
  {FAN_PWM_PIN, 0, true,  FAN_HIGH, "HIGH", topicFanStatus},
};
const size_t initCommandCount = sizeof(initCommands) / sizeof(initCommands[0]);


//########################################################################
//#   Command Structure
//#     ctrlTopic   - the MQTT topic this command is received on
//#     command     - expected payload (ON/OFF for switches, speed for fan)
//#     statusTopic - topic to publish the resulting state to
//########################################################################
struct {
  const char* ctrlTopic;
  const char* command;
  uint8_t espPin;
  bool state;
  bool isAnalog;
  uint8_t analogValue;
  const char* feedback;
  const char* statusTopic;
} commands[] = {
  {topicKriaCtrl,   "ON",   KRIA_PIN,    1, false, 0,        "ON",   topicKriaStatus},
  {topicKriaCtrl,   "OFF",  KRIA_PIN,    0, false, 0,        "OFF",  topicKriaStatus},
  {topicPynqz1Ctrl, "ON",   PYNQZ1_PIN,  1, false, 0,        "ON",   topicPynqz1Status},
  {topicPynqz1Ctrl, "OFF",  PYNQZ1_PIN,  0, false, 0,        "OFF",  topicPynqz1Status},
  {topicPluto1Ctrl, "ON",   PLUTO1_PIN,  1, false, 0,        "ON",   topicPluto1Status},
  {topicPluto1Ctrl, "OFF",  PLUTO1_PIN,  0, false, 0,        "OFF",  topicPluto1Status},
  {topicPluto2Ctrl, "ON",   PLUTO2_PIN,  1, false, 0,        "ON",   topicPluto2Status},
  {topicPluto2Ctrl, "OFF",  PLUTO2_PIN,  0, false, 0,        "OFF",  topicPluto2Status},
  {topicFanCtrl,    "OFF",  FAN_PWM_PIN, 0, true,  FAN_OFF,  "OFF",  topicFanStatus},
  {topicFanCtrl,    "LOW",  FAN_PWM_PIN, 0, true,  FAN_LOW,  "LOW",  topicFanStatus},
  {topicFanCtrl,    "MED",  FAN_PWM_PIN, 0, true,  FAN_MED,  "MED",  topicFanStatus},
  {topicFanCtrl,    "HIGH", FAN_PWM_PIN, 0, true,  FAN_HIGH, "HIGH", topicFanStatus},
};
const size_t commandCount = sizeof(commands) / sizeof(commands[0]);


//########################################################################
//#   Enter your sensitive data in the arduino_secrets.h
//########################################################################
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;



//########################################################################
//#   To connect with SSL/TLS:
//#   1) Change WiFiClient to WiFiSSLClient.
//#   2) Change port value from 1883 to 8883.
//#   3) Change broker value to a server with a known SSL/TLS 
//#       root certificate flashed in the WiFi module.
//########################################################################

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char  broker[]        = "nuc-sdr";
int         port            = 1883;

// MQTT Settings
const int  MQTT_QoS         = 1;  // 0,1,2
const bool MQTT_RETAIN      = false; 
const bool MQTT_DUP         = false; 

// Connection management
const int   WIFI_TIMEOUT_SEC = 30;  // Try connecting for 30 seconds
const int   MQTT_TIMEOUT_SEC = 10;  // Try connecting for 10 seconds


// RSSI publish timing
unsigned long lastRssiPublish = 0;
const unsigned long RSSI_PUBLISH_INTERVAL_MS = 10000; // publish every 60s




//########################################################################
//#   WiFi Connection
//########################################################################
void connectWiFi() {
  Log.info("Attempting to connect to WPA SSID: %s" CR, ssid);
  WiFi.begin(ssid, pass);
  
  int timeout = 0;
  while ((WiFi.status() != WL_CONNECTED) && (timeout < WIFI_TIMEOUT_SEC)) {
    Log.info("WiFi Connection Duration: %d sec" CR, timeout);
    delay(1000);
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Log.info("WiFi connected! IP: %s" CR, WiFi.localIP().toString());
  } else {
    Log.error("WiFi connection failed!" CR);
  }
}


//########################################################################
//#   MQTT Connection
//########################################################################
void connectMQTT() {
  Log.info("Attempting to connect to the MQTT broker: %s" CR, broker);
  
  int timeout = 0;
  while ((!mqttClient.connect(broker, port)) && (timeout < MQTT_TIMEOUT_SEC)) {
    //    #define MQTT_CONNECTION_REFUSED            -2
    //    #define MQTT_CONNECTION_TIMEOUT            -1
    //    #define MQTT_SUCCESS                        0
    //    #define MQTT_UNACCEPTABLE_PROTOCOL_VERSION  1
    //    #define MQTT_IDENTIFIER_REJECTED            2
    //    #define MQTT_SERVER_UNAVAILABLE             3
    //    #define MQTT_BAD_USER_NAME_OR_PASSWORD      4
    //    #define MQTT_NOT_AUTHORIZED                 5

    Log.warning("MQTT connection failed! Error code = %d" CR, mqttClient.connectError());
    Log.info("MQTT Connection Duration: %d sec" CR, timeout);
    delay(1000);
    timeout++;
  }

  if (mqttClient.connected()) {
    Log.info("MQTT connected to broker!" CR);
    mqttClient.onMessage(onMqttMessage);
    for (size_t i = 0; i < ctrlTopicCount; i++) {
      Log.info("Subscribing to: %s" CR, ctrlTopics[i]);
      mqttClient.subscribe(ctrlTopics[i], MQTT_QoS);
    }
    publishWiFiRSSI();
  } else {
    Log.error("MQTT connection failed!" CR);
  }
}


//########################################################################
//#   Setup
//########################################################################
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);
  Log.info("Setup Started..." CR);

  
  // Initialize pin modes and send initial MQTT status
  for (size_t i = 0; i < initCommandCount; i++) {
    pinMode(initCommands[i].espPin, OUTPUT);
    if (initCommands[i].isAnalog) {
      analogWriteResolution(initCommands[i].espPin, PWM_RES);
      analogWriteFrequency(initCommands[i].espPin, PWM_FREQ);
      analogWrite(initCommands[i].espPin, initCommands[i].analogValue);
    } else {
      digitalWrite(initCommands[i].espPin, initCommands[i].state);
    }
  }

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("puch-Rack-Controller");

  // MQTT authentication
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);

  // Connect to WiFi and MQTT
  connectWiFi();
  connectMQTT();

  // Send initial status to MQTT server
  if (mqttClient.connected()) {
    Log.info("Publishing initial status..." CR);
    for (size_t i = 0; i < initCommandCount; i++) {
      mqttClient.beginMessage(initCommands[i].statusTopic, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
      mqttClient.print(initCommands[i].feedback);
      mqttClient.endMessage();
      Log.info("Published to %s: %s" CR, initCommands[i].statusTopic, initCommands[i].feedback);
    }
    publishWiFiRSSI();
  }
}



//########################################################################
//#   Handle WiFi/MQTT Reconnection
//########################################################################
void handleReconnection() {
  
  if (WiFi.status() != WL_CONNECTED) {
    Log.warning("WiFi disconnected. Attempting to reconnect..." CR);
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqttClient.connected()) {
        Log.warning("MQTT disconnected. Attempting to reconnect..." CR);
        connectMQTT();
      }
    }
  }
}

// Publish WiFi RSSI (dBm) to topicWifiRssi as a plain numeric value
void publishWiFiRSSI() {
  if (!mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  long rssi = WiFi.RSSI();
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", rssi);

  mqttClient.beginMessage(topicWifiRssi, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
  mqttClient.print(buf);
  mqttClient.endMessage();

  Log.info("Published to %s: %s" CR, topicWifiRssi, buf);
}




//########################################################################
//#   Loop
//########################################################################
void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // Check and handle WiFi/MQTT connection
  handleReconnection();

  // Periodically publish RSSI
  if ((millis() - lastRssiPublish) >= RSSI_PUBLISH_INTERVAL_MS) {
    publishWiFiRSSI();
    lastRssiPublish = millis();
  }
}




//########################################################################
//#   Process MQTT Messages
//########################################################################
void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  Log.info("Received message with topic: %s, bytes: %d" CR, topic.c_str(), messageSize);

  String content = "";
  for (size_t i = 0; i < messageSize; i++) {
    content.concat((char)mqttClient.read());
  }

  bool found = false;
  for (size_t i = 0; i < commandCount; i++) {
    if (topic == commands[i].ctrlTopic && content == commands[i].command) {
      Log.info("Executing: %s -> %s" CR, topic.c_str(), content.c_str());

      if (commands[i].isAnalog) {
        analogWrite(commands[i].espPin, commands[i].analogValue);
      } else {
        digitalWrite(commands[i].espPin, commands[i].state);
      }

      mqttClient.beginMessage(commands[i].statusTopic, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
      mqttClient.print(commands[i].feedback);
      mqttClient.endMessage();

      found = true;
      break;
    }
  }

  if (!found) {
    Log.error("Unknown command on topic %s: %s" CR, topic.c_str(), content.c_str());
  }
}
