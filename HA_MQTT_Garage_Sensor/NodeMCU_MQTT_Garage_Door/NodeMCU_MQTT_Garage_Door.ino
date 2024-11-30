//#############################################################################################
//#############################################################################################
//#
//#   The MIT License (MIT)
//#   
//#   Copyright (c) 2024 http://odelay.io 
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
//#   Description : 
//#           
//#
//#   Version History:
//#   
//#       Date        Description
//#     -----------   -----------------------------------------------------------------------
//#      2024-10-05    Original Creation
//#
//###########################################################################################

#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "arduino_secrets.h"
#include "UltrasonicHCSR04.h"
#include "Wire.h"
#include "SHT31.h"
#include "ArduinoLog.h"


//*********************************************************************
//    System Parameters
//*********************************************************************

// Log Level (see note above)
// Set to VERBOSE during development, then SILENT for operation
#define LOG_LEVEL LOG_LEVEL_VERBOSE
//#define LOG_LEVEL LOG_LEVEL_SILENT

#define SHT31_ADDRESS   0x44
SHT31 sht;

// Setup HC-SR04 Device
const int trigPin = 12; // GPIO-12
const int echoPin = 13; // GPIO-13
UltrasonicHCSR04 distSensor(trigPin, echoPin);

// arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;


const char broker[]       = "nuc-sdr";
int        port           = 1883;
const char topicStatus[]  = "garage/door";
const char topicTemp[]    = "garage/temperature";
const char topicHumid[]   = "garage/humidity";
const char topicRSSI[]    = "garage/rssi";
uint8_t    MQTT_QoS       = 0; // 0:Best Effort, 1:Received ACK, 2:SND/ACK/SND/ACK
bool       MQTT_RETAIN    = true;
bool       MQTT_DUP       = false;

const long interval       = 2000; // update rate 2sec
const int NUM_RETRIES     = 10; // Number of retries for Wifi MQTT
int retries               = 0;

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate 
//    flashed in the WiFi module.
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);



//*********************************************************************
//    Check WiFi & MQTT status. return 0 if connected, else 1
//*********************************************************************
int statusWifiMQTT() {
 
  // Check WiFi
  if(WiFi.status() != WL_CONNECTED) {
    Log.warning(F("STATUS: WiFi connection failed" CR));
    return 1;
  }
  Log.info("STATUS: You're connected to the network: %s" CR, WiFi.localIP().toString());
  Log.info("STATUS: WiFi RSSI: %l dBm" CR, WiFi.RSSI());

  // Check MQTT
  if(!mqttClient.connected()) {
    Log.warning(F("STATUS: MQTT connection failed" CR));
    return 1;
  }

  Log.info(F("STATUS: You're connected to the MQTT broker!" CR CR));
  return 0;

}


//*********************************************************************
//    Connect to WiFi and MQTT.  Return 0 if successful, else 1
//*********************************************************************
int connectWifiMQTT() {


  // attempt to connect to WiFi network:
  Log.info("Attempting to connect to WPA SSID: %s, " CR,ssid);
  WiFi.begin(ssid, pass);

  retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < NUM_RETRIES)) {
    // failed, retry
    Log.info("Retry: %d" CR, retries);
    delay(1000);
    ++retries;
    //WiFi.begin(ssid, pass);
  }


  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("Garage-Monitor-001");

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);
  Log.info("Attempting to connect to the MQTT broker: %s" CR, broker);

  // Connect to MQTT Server
  retries = 0;
  while( (!mqttClient.connect(broker, port)) && (retries < NUM_RETRIES)) {
    Log.warning("MQTT connection failed! Error code = %s" CR, mqttClient.connectError());
    Log.info("Retry: %d" CR, retries);
    delay(1000);
    ++retries;
  }


  // Return 0 if everything was successful
  return statusWifiMQTT();

}



//*********************************************************************
//    System Setup
//*********************************************************************
float distance = 0.0;
unsigned long previousMillis = 0;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);

  // Connect to WiFi then MQTT broker
  while(connectWifiMQTT() != 0) {
    Log.info("Connecting to server..." CR);
  }

  // setup Temperature and Humidity SHT31-D
  Wire.begin();
  Wire.setClock(50000);
  sht.begin();

}



//*********************************************************************
//    Main Loop()
//*********************************************************************
void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // to avoid having delays in loop, we'll use the strategy from BlinkWithoutDelay
  // see: File -> Examples -> 02.Digital -> BlinkWithoutDelay for more info
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    // save the last time a message was sent
    previousMillis = currentMillis;

    // Check if connected to server, and reconnect
    while(statusWifiMQTT() == 1) {
      Log.info(F("Connecting to server..." CR));
      connectWifiMQTT();
    }

    // Check if measurement is in range
    float d = distSensor.measureDistanceInches();
    if(d < 200.0 & d >= 0.0) distance = d;

    Log.info("Distance: %F in" CR, distance);

    // Send Garage Distance to HA-MQTT
    mqttClient.beginMessage(topicStatus, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
    mqttClient.print(distance);
    mqttClient.endMessage();

    // Read Temperature and Humidtiy
    bool sht_success = sht.read(false);
    if(sht_success == false) {
      Log.fatal("FAILED: Unable to Read SHT, trying again..." CR);
      bool sht_success = sht.read(false);
    }
    float sht_temp = sht.getFahrenheit();
    float sht_humid = sht.getHumidity();
    Log.info("Temperature: %F F, Humidity: %F %c" CR, sht_temp, sht_humid,'%');

    // Sent Temperature and Humidity to HA-MQTT
    mqttClient.beginMessage(topicTemp, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
    mqttClient.print(sht_temp);
    mqttClient.endMessage();

    mqttClient.beginMessage(topicHumid, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
    mqttClient.print(sht_humid);
    mqttClient.endMessage();

    mqttClient.beginMessage(topicRSSI, MQTT_RETAIN, MQTT_QoS, MQTT_DUP);
    mqttClient.print(WiFi.RSSI());
    mqttClient.endMessage();
  }
}
