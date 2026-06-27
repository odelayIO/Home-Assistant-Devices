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
//#      2026-06-26    Added WiFi RSSI
//#      2026-06-27    Per-device MQTT topics (puch_rack/<device>/control|status)
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
//#   Per-Device Configuration
//#
//#   Control topic payload:
//#     Digital devices : "ON" or "OFF"
//#     Fan             : "OFF", "LOW", "MED", or "HIGH"
//#
//#   Status topic publishes the same payload back as feedback.
//########################################################################
struct Device {
  const char* ctrlTopic;
  const char* statusTopic;
  uint8_t     espPin;
  bool        isAnalog;
  const char* initPayload;    // payload published on boot
  uint8_t     initAnalogVal;  // used when isAnalog == true
  bool        initState;      // used when isAnalog == false
} devices[] = {
  {"puch_rack/kria/control",   "puch_rack/kria/status",   KRIA_PIN,    false, "OFF",  0,        LOW},
  {"puch_rack/pynqz1/control", "puch_rack/pynqz1/status", PYNQZ1_PIN,  false, "OFF",  0,        LOW},
  {"puch_rack/pluto1/control", "puch_rack/pluto1/status", PLUTO1_PIN,  false, "OFF",  0,        LOW},
  {"puch_rack/pluto2/control", "puch_rack/pluto2/status", PLUTO2_PIN,  false, "OFF",  0,        LOW},
  {"puch_rack/fan/control",    "puch_rack/fan/status",    FAN_PWM_PIN, true,  "HIGH", FAN_HIGH, LOW},
};
const size_t deviceCount = sizeof(devices) / sizeof(devices[0]);


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
const char  topicWiFi[]     = "puch_rack/WiFiRSSI";

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
    Log.warning("MQTT connection failed! Error code = %d" CR, mqttClient.connectError());
    Log.info("MQTT Connection Duration: %d sec" CR, timeout);
    delay(1000);
    timeout++;
  }

  if (mqttClient.connected()) {
    Log.info("MQTT connected to broker!" CR);
    mqttClient.onMessage(onMqttMessage);

    // Subscribe to each device's control topic
    for (size_t i = 0; i < deviceCount; i++) {
      Log.info("Subscribing to control topic: %s" CR, devices[i].ctrlTopic);
      mqttClient.subscribe(devices[i].ctrlTopic, MQTT_QoS);
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
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);
  Log.info("Setup Started..." CR);


  mqttClient.setId("puch-Rack-Controller");
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);

  connectWiFi();
  connectMQTT();

  // Publish boot state to each device's status topic
  if (mqttClient.connected()) {
    Log.info("Publishing initial status..." CR);
    for (size_t i = 0; i < deviceCount; i++) {
      mqttClient.beginMessage(devices[i].statusTopic, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
      mqttClient.print(devices[i].initPayload);
      mqttClient.endMessage();
      Log.info("Published [%s]: %s" CR, devices[i].statusTopic, devices[i].initPayload);
    }
  }

  // Initialize pins and apply boot state
  for (size_t i = 0; i < deviceCount; i++) {
    pinMode(devices[i].espPin, OUTPUT);
    if (devices[i].isAnalog) {
      analogWriteResolution(devices[i].espPin, PWM_RES);
      analogWriteFrequency(devices[i].espPin, PWM_FREQ);
      analogWrite(devices[i].espPin, devices[i].initAnalogVal);
      //analogWrite(devices[i].espPin, FAN_HIGH);
      Log.info("Setting FAN to init speed: %d" CR, devices[i].initAnalogVal);
    } else {
      digitalWrite(devices[i].espPin, devices[i].initState);
      Log.info("Setting Pin: %d, to init value: %d" CR, devices[i].espPin, devices[i].initState);
    }
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

// Publish WiFi RSSI (dBm) to topicWiFi
void publishWiFiRSSI() {
  mqttClient.beginMessage(topicWiFi, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
  mqttClient.print(WiFi.RSSI());
  mqttClient.endMessage();

  Log.info("Published WiFiRSSI: %d" CR, WiFi.RSSI());
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
//#
//#   Routes incoming message to the correct device by topic, then
//#   applies the payload:
//#     Digital : "ON" → HIGH, "OFF" → LOW
//#     Fan     : "OFF" | "LOW" | "MED" | "HIGH"
//########################################################################
void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  Log.info("Received [%s], bytes: %d" CR, topic.c_str(), messageSize);

  String payload = "";
  for (size_t i = 0; i < messageSize; i++) {
    payload.concat((char)mqttClient.read());
  }

  // Find matching device by control topic
  for (size_t i = 0; i < deviceCount; i++) {
    if (topic != devices[i].ctrlTopic) continue;

    if (devices[i].isAnalog) {
      // Fan speed control
      uint8_t val;
      if      (payload == "OFF")  val = FAN_OFF;
      else if (payload == "LOW")  val = FAN_LOW;
      else if (payload == "MED")  val = FAN_MED;
      else if (payload == "HIGH") val = FAN_HIGH;
      else {
        Log.error("Unknown fan payload: %s" CR, payload.c_str());
        return;
      }
      analogWrite(devices[i].espPin, val);
    } else {
      // Digital switch control
      if      (payload == "ON")  digitalWrite(devices[i].espPin, HIGH);
      else if (payload == "OFF") digitalWrite(devices[i].espPin, LOW);
      else {
        Log.error("Unknown switch payload: %s" CR, payload.c_str());
        return;
      }
    }

    Log.info("Executed [%s]: %s" CR, topic.c_str(), payload.c_str());

    // Echo payload back on the status topic
    mqttClient.beginMessage(devices[i].statusTopic, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
    mqttClient.print(payload);
    mqttClient.endMessage();
    return;
  }

  Log.error("No device matched topic: %s" CR, topic.c_str());
}
