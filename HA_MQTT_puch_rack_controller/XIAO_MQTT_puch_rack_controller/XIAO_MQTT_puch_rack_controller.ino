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
//#   Fan Constants for PWM
//########################################################################
uint8_t FAN_OFF   = 0;
uint8_t FAN_LOW   = 63;
uint8_t FAN_MED   = 127;
uint8_t FAN_HIGH  = 255;

#define PWM_FREQ 25000   // 25 kHz (PC fan spec)
#define PWM_RES  8       // 8-bit (0–255)
uint8_t FAN_PWM_PIN = D0; 



//########################################################################
//#   Command Structure
//########################################################################
struct {
  const char* command;
  uint8_t relayPin;
  bool state;
  const char* feedback;
} commands[] = {
  {"KRIA-ON",     D10, 1, "KRIA-ON"},
  {"KRIA-OFF",    D10, 0, "KRIA-OFF"},
  {"PYNQZ1-ON",   D9, 1, "PYNQZ1-ON"},
  {"PYNQZ1-OFF",  D9, 0, "PYNQZ1-OFF"},
  {"PLUTO-1-ON",  D8, 1, "PLUTO-1-ON"},
  {"PLUTO-1-OFF", D8, 0, "PLUTO-1-OFF"},
  {"PLUTO-2-ON",  D7, 1, "PLUTO-2-ON"},
  {"PLUTO-2-OFF", D7, 0, "PLUTO-2-OFF"},
  {"FAN-OFF", FAN_PWM_PIN, 0, "FAN-OFF"},
  {"FAN-LOW", FAN_PWM_PIN, 0, "FAN-LOW"},
  {"FAN-MED", FAN_PWM_PIN, 0, "FAN-MED"},
  {"FAN-HIGH", FAN_PWM_PIN, 0, "FAN-HIGH"},
};
const size_t commandCount = sizeof(commands) / sizeof(commands[0]);


//########################################################################
//#   Log Level (see note above)
//#   Set to VERBOSE during development, then SILENT for operation
//########################################################################

#define LOG_LEVEL LOG_LEVEL_VERBOSE
//#define LOG_LEVEL LOG_LEVEL_SILENT



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
const char  topicStatus[]   = "puch_rack/status";
const char  topicCtrl[]     = "puch_rack/control";

// MQTT Settings
const int   MQTT_QoS        = 1;  // 0,1,2
const bool   MQTT_RETAIN    = false; 
const bool   MQTT_DUP       = false; 
const long UPDATE_RATE_MS   = 2000; // Update Rate

// Connection management
const long  WIFI_RECONNECT_INTERVAL_MS = 30000;  // Try WiFi reconnection every 30 seconds
const long  MQTT_RECONNECT_INTERVAL_MS = 10000;  // Try MQTT reconnection every 10 seconds
const long  CONNECTION_CHECK_INTERVAL_MS = 5000; // Check connection status every 5 seconds
const int   WIFI_TIMEOUT_SEC = 20;  
const int   MQTT_TIMEOUT_SEC = 20;  
unsigned long lastWifiReconnectAttempt = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastConnectionCheck = 0;



//########################################################################
//#   WiFi Connection
//########################################################################
void connectWiFi() {
  Log.info("Attempting to connect to WPA SSID: %s" CR, ssid);
  WiFi.begin(ssid, pass);
  
  int timeout = 0;
  while ((WiFi.status() != WL_CONNECTED) && (timeout < WIFI_TIMEOUT_SEC)) {
    Log.info(".");
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
    Log.info("Second: %d" CR, timeout);
    delay(1000);
    timeout++;
  }

  if (mqttClient.connected()) {
    Log.info("MQTT connected to broker!" CR);
    // set the message receive callback
    mqttClient.onMessage(onMqttMessage);
    // subscribe to a topic
    Log.info("Subscribing to topic: %s" CR, topicCtrl);
    mqttClient.subscribe(topicCtrl, MQTT_QoS);
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

  // Initialize pin modes
  for (size_t i = 0; i < commandCount; i++) {
    pinMode(commands[i].relayPin, OUTPUT);
    digitalWrite(commands[i].relayPin, LOW); // or HIGH depending on your relay logic
  }

  // Fan PWM Configuration
  pinMode(FAN_PWM_PIN, OUTPUT);
  analogWriteResolution(FAN_PWM_PIN, PWM_RES);
  analogWriteFrequency(FAN_PWM_PIN, PWM_FREQ);

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("puch-Rack-Controller");

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);

  // Connect to WiFi and MQTT
  connectWiFi();
  connectMQTT();
}



//########################################################################
//#   Handle WiFi Reconnection
//########################################################################
void handleWiFiReconnection() {
  unsigned long currentMillis = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastWifiReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
      Log.warning("WiFi disconnected. Attempting to reconnect..." CR);
      lastWifiReconnectAttempt = currentMillis;
      connectWiFi();
      
      // Reset MQTT reconnection timer to attempt connection immediately if WiFi reconnected
      if (WiFi.status() == WL_CONNECTED) {
        lastMqttReconnectAttempt = 0;
      }
    }
  }
}


//########################################################################
//#   Handle MQTT Reconnection
//########################################################################
void handleMqttReconnection() {
  unsigned long currentMillis = millis();
  
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      if (currentMillis - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
        Log.warning("MQTT disconnected. Attempting to reconnect..." CR);
        lastMqttReconnectAttempt = currentMillis;
        connectMQTT();
      }
    } else {
      // Connected to both WiFi and MQTT - call poll() to maintain connection
      mqttClient.poll();
    }
  }
}


//########################################################################
//#   Loop
//########################################################################
void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // Check and handle WiFi connection
  handleWiFiReconnection();
  
  // Check and handle MQTT connection
  handleMqttReconnection();

}




//########################################################################
//#   Process MQTT Messages
//########################################################################
void onMqttMessage(int messageSize) {
  Log.info("Received message with topic: %s, bytes: %d" CR,mqttClient.messageTopic().c_str(),messageSize);

  // Create a string with the MQTT message
  String content = "";
  for (size_t i = 0; i < messageSize; i++) {
    content.concat((char)mqttClient.read());
  }

  bool found = false;
  for (size_t i = 0; i < commandCount; i++) {
    if (content == commands[i].command) {
      Log.info("Executing: %s" CR,commands[i].feedback);

      // Drive pins or set fan speed
      if(content == "FAN-OFF") {
        analogWrite(FAN_PWM_PIN,255-FAN_OFF);
      } else if(content == "FAN-LOW") {
        analogWrite(FAN_PWM_PIN,255-FAN_LOW);
      } else if(content == "FAN-MED") {
        analogWrite(FAN_PWM_PIN,255-FAN_MED);
      } else if(content == "FAN-HIGH") {
        analogWrite(FAN_PWM_PIN,255-FAN_HIGH);
      } else {
        // Write the Pin connected to Relay Switch
        digitalWrite(commands[i].relayPin, commands[i].state); 
      }
     
      // Send Feedback message over MQTT
      mqttClient.beginMessage(topicStatus, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
      mqttClient.print(commands[i].feedback);
      mqttClient.endMessage();
      
      found = true;
      break;
    }
  }
  
  if (!found) {
    Log.warning("Unknown command: %s" CR, content);
  }
}
