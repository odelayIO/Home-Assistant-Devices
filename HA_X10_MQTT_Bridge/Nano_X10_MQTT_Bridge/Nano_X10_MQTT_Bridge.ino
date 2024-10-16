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
#include <x10.h>
#include <x10constants.h>

#define zcPin D2
#define dataPin D3

// set up a new x10 instance:
x10 myHouse;



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
const char topicStatus[]  = "x10/status";
const char topicSwitch[]  = "x10/switch";

const long interval = 2000;
unsigned long previousMillis = 0;

int count = 0;

// MQTT Settings
const int qos     = 1;
const bool retain = false;
const bool dup    = false;


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
  mqttClient.setId("X10-Bridge");

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

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);

  Serial.print("Subscribing to topic: ");
  Serial.println(topicSwitch);
  Serial.println();

  // subscribe to a topic
  mqttClient.subscribe(topicSwitch, qos);

  // topics can be unsubscribed using:
  // mqttClient.unsubscribe(topic);

  Serial.print("Waiting for messages on topic: ");
  Serial.println(topicSwitch);
  Serial.println();

  // Connect to X10 module
	myHouse.init(zcPin, dataPin);
	Serial.println(myHouse.version());

}

void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();
  
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

  delay(5000);
}

void onMqttMessage(int messageSize) {
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // Create a string with the MQTT message
  String content = "";
  for (size_t i = 0; i < messageSize; i++) {
    content.concat((char)mqttClient.read());
  }
   
  // Check the message with commands
  if(content == "DININGROOM-ON") {
    Serial.println("Turning on Dining Room Lights...");
    myHouse.write(HOUSE_A,ALL_LIGHTS_ON,3);
    mqttClient.beginMessage(topicStatus, false, qos, false);
    mqttClient.print("DININGROOM-ON");
    mqttClient.endMessage();
  } 
  if(content == "DININGROOM-OFF") {
    Serial.println("Turning off Dining Room Lights...");
    myHouse.write(HOUSE_A,ALL_UNITS_OFF,3);
    mqttClient.beginMessage(topicStatus, false, qos, false);
    mqttClient.print("DININGROOM-OFF");
    mqttClient.endMessage();
  } 
  if(content == "LIVINGROOM-ON") {
    Serial.println("Turning on Living Room Lights...");
    myHouse.write(HOUSE_B,ALL_LIGHTS_ON,3);
    mqttClient.beginMessage(topicStatus, false, qos, false);
    mqttClient.print("LIVINGROOM-ON");
    mqttClient.endMessage();
  } 
  if(content == "LIVINGROOM-OFF") {
    Serial.println("Turning off Living Room Lights...");
    myHouse.write(HOUSE_B,ALL_UNITS_OFF,3);
    mqttClient.beginMessage(topicStatus, false, qos, false);
    mqttClient.print("LIVINGROOM-OFF");
    mqttClient.endMessage();
  } 

  Serial.print(content);
  Serial.println();
}
