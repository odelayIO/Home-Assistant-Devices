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
//#
//#           
//#
//#   Version History:
//#   
//#       Date        Description
//#     -----------   -----------------------------------------------------------------------
//#      2024-10-13    Original Creation
//#
//###########################################################################################

#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "arduino_secrets.h"
#include "Wire.h"
#include "SHT31.h"
#include "ArduinoLog.h"


//*********************************************************************
//    System Parameters
//*********************************************************************

// Log Level (see note above)
// Set to VERBOSE during development, then SILENT for operation
//#define LOG_LEVEL LOG_LEVEL_VERBOSE
#define LOG_LEVEL LOG_LEVEL_SILENT

// MQTT Broker
// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate 
//    flashed in the WiFi module.
const char broker[]       = "nuc-sdr";
int        port           = 1883;
const char topicTemp[]    = "wine_frig/main/temperature";
const char topicHumid[]   = "wine_frig/main/humidity";
uint8_t    MQTT_QoS       = 1; // 0:Best Effort, 1:Received ACK, 2:SND/ACK/SND/ACK

// Network Settings found in "arduino_secrets.h" file
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;


// Update Rate in ms
const long UPDATE_RATE_MS = 2000;

// I2C Address for SHT31 device.  Not need to update
#define SHT31_ADDRESS   0x44
SHT31 sht;



//*********************************************************************
//    Setup Function
//*********************************************************************

// Define WiFi
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

unsigned long previousMillis = 0;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);

  // attempt to connect to WiFi network:
  Log.info(CR "Attempting to connect to WPA SSID: %s" CR, ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    // failed, retry
    Log.info(F("."));
    delay(1000);
  }

  Log.info(CR "You're connected to the network: %s" CR, WiFi.localIP());

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("Main-Wine-Frig");
  

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);

  Log.info("Attempting to connect to the MQTT broker: %s" CR, broker);

  if (!mqttClient.connect(broker, port)) {
    Log.warning("MQTT connection failed! Error code = %s" CR, mqttClient.connectError());

    while (1);
  }

  Log.info(F("You're connected to the MQTT broker!" CR));

  // setup Temperature and Humidity SHT31-D
  Wire.begin();
  Wire.setClock(100000);
  sht.begin();
}

//*********************************************************************
//    Loop Function
//*********************************************************************
void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // to avoid having delays in loop, we'll use the strategy from BlinkWithoutDelay
  // see: File -> Examples -> 02.Digital -> BlinkWithoutDelay for more info
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= UPDATE_RATE_MS) {
    // save the last time a message was sent
    previousMillis = currentMillis;

    // Read Temperature and Humidtiy
    bool sht_success = sht.read(false);
    sht.requestData(); 
    if(sht_success == false) {
      Log.fatal(F("FAILED: Unable to Read SHT" CR));
    }
    float sht_temp = sht.getFahrenheit();
    float sht_humid = sht.getHumidity();
    Log.info("Temp: %F, Humidity %F" CR,sht_temp, sht_humid);

    // Send Temperature and Humidity to HA-MQTT
    mqttClient.beginMessage(topicTemp, false, MQTT_QoS, false);
    mqttClient.print(sht_temp);
    mqttClient.endMessage();

    mqttClient.beginMessage(topicHumid, false, MQTT_QoS, false);
    mqttClient.print(sht_humid);
    mqttClient.endMessage();

    // Not sure if this is required, but checking
    // if the ESP32 is connected to MQTT broker.
    // When not connected, then reconnects.
    Log.info(F("Check MQTT Connection..." CR));
    if(!mqttClient.connected()) {
      Log.warning(F("Disconnected, trying to connect..."));
      if (!mqttClient.connect(broker, port)) {
        Log.fatal(F("FAILED to reconnect to MQTT" CR));
      } else {
        Log.info(F("Successfully reconnected to MQTT" CR));
      }
    } else {
      Log.info(F("Connected." CR));
    }
  }
}
