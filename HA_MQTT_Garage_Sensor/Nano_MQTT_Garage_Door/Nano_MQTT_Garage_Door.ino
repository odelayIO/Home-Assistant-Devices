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


#define SHT31_ADDRESS   0x44
SHT31 sht;

// Setup HC-SR04 Device
const int trigPin = D3; // GPIO-6
const int echoPin = D2; // GPIO-5
UltrasonicHCSR04 distSensor(trigPin, echoPin);

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate 
//    flashed in the WiFi module.

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


const char broker[]       = "nuc-sdr";
int        port           = 1883;
const char topicStatus[]  = "garage/door";
const char topicTemp[]    = "garage/temperature";
const char topicHumid[]   = "garage/humidity";

const long interval = 2000;
unsigned long previousMillis = 0;


void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);

  // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(1000);
  }

  Serial.print("You're connected to the network: ");
  Serial.println(WiFi.localIP());

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("Garage-Monitor");
  

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  // setup Temperature and Humidity SHT31-D
  Wire.begin();
  Wire.setClock(100000);
  sht.begin();
}

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

    float distance = distSensor.measureDistanceInches();
    //float distance = 0.0;

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" in");

    //.beginMessage(topic, false, 0, false);
    // QoS=1, default is 0
    mqttClient.beginMessage(topicStatus, false, 0, false);
    mqttClient.print(distance);
    mqttClient.endMessage();

    // Read Temperature and Humidtiy
    bool sht_success = sht.read(false);
    sht.requestData(); 
    if(sht_success == false) {
      Serial.println("FAILED: Unable to Read SHT");
    }
    float sht_temp = sht.getFahrenheit();
    Serial.print(sht_temp);
    Serial.print(" F\t");
    float sht_humid = sht.getHumidity();
    Serial.print(sht_humid);
    Serial.println(" %");

    // Sent Temperature and Humidity to HA-MQTT
    mqttClient.beginMessage(topicTemp, false, 0, false);
    mqttClient.print(sht_temp);
    mqttClient.endMessage();

    mqttClient.beginMessage(topicHumid, false, 0, false);
    mqttClient.print(sht_humid);
    mqttClient.endMessage();

    Serial.print("Check MQTT Connection: ");
    if(!mqttClient.connected()) {
      Serial.println("Disconnected, trying to connect...");
      if (!mqttClient.connect(broker, port)) {
        Serial.println("FAILED to reconnect to MQTT");
      } else {
        Serial.println("Successfully reconnected to MQTT");
      }
    } else {
      Serial.println("Connected");
    }
    Serial.println(" ");
  }
}
