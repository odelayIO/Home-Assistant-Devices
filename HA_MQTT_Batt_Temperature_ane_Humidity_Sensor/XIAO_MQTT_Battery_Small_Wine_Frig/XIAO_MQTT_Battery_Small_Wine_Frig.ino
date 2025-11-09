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
//#      2024-10-24    Implemented Deep Sleep
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

// Update Rate, how long should stay in Deep Sleep
#define UPDATE_RATE_MIN     10 // Minutes
#define uS_TO_MIN_FACTOR    1000000*60

// Ground Pin 8 and reboot board to stop from going to deep sleep
#define MONITOR_PIN 8  // GPIO8 / D8


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
const char topicTemp[]    = "wine_frig/small/temperature";
const char topicHumid[]   = "wine_frig/small/humidity";
const char topicBatt[]    = "wine_frig/small/battery";
uint8_t    MQTT_QoS       = 1; // 0:Best Effort, 1:Received ACK, 2:SND/ACK/RSP/ACK

// Network Settings found in "arduino_secrets.h" file
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;


// I2C Address for SHT31 device.  Not need to update
#define SHT31_ADDRESS   0x44
SHT31 sht;

//*********************************************************************
//    End of System Parameters
//*********************************************************************



// Define WiFi
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


//*********************************************************************
//    Update HA Function (~80ms to send MQTT message)
//*********************************************************************
void update_HA() {

  // Uncomment the below lines of code to measure MQTT message duration
  // Also uncomment at end of function too.
  //unsigned long currentMillis = millis();
  
  
  // Read Temperature and Humidtiy
  bool sht_success = sht.read(false); // read slow
  //sht.requestData();  This should be removed...
  if(sht_success == false) {
    Log.fatal(F("FAILED: Unable to Read SHT, trying again..." CR));
    bool sht_success = sht.read(false); // read slow
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

  // Read Battery level and sent to HA-MQTT
  uint32_t batt_level = 0;
  for (int i = 0; i<16; i++) {
    batt_level = batt_level + analogReadMilliVolts(A0);
  }
  float batt_lvl_float = ((batt_level / 16 / 1000.0) / 2.5) * 100.0;
  Log.info("Analog mV Raw Read: %d" CR,analogReadMilliVolts(A0));
  Log.info("Analog Raw Read: %d" CR,analogRead(A0));
  Log.info("Battery Level: %F" CR,batt_lvl_float);
  mqttClient.beginMessage(topicBatt, false, MQTT_QoS, false);
  mqttClient.print(batt_lvl_float);
  mqttClient.endMessage();



  // Uncomment the below lines of code to measure MQTT message duration
  //unsigned long message_time =  millis() - currentMillis;
  //Serial.print("Message Time: ");
  //Serial.print(message_time);
  //Serial.println(" ms");
}



//*********************************************************************
//    Setup Function (main function)
//*********************************************************************
void setup() {

  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);

  // Configure pin as input with internal pull-up
  pinMode(MONITOR_PIN, INPUT_PULLUP);
 
  // Loop while the pin is LOW
  while (digitalRead(MONITOR_PIN) == LOW) {
    // Optional: print status
    Serial.println("Stopped HA Sensor boot...");
    delay(1000);
  }

  // setup Temperature and Humidity SHT31-D
  Wire.begin();
  Wire.setClock(50000);
  sht.begin();

  // Configure ESP32 Sleep Wakeup Timer
  esp_sleep_enable_timer_wakeup(UPDATE_RATE_MIN * uS_TO_MIN_FACTOR);


  // attempt to connect to WiFi network:
  Log.info(CR "Attempting to connect to WPA SSID: %s" CR, ssid);
  WiFi.begin(ssid, pass);
  int i=0;
  while(WiFi.status() != WL_CONNECTED) {
    // failed, retry
    Log.info(F("."));
    delay(1000);
    if(i>20) {
      Log.warning("Not connected to WiFi!");
      break;
    } else {
      ++i;
    }
  }
  if(WiFi.status() == WL_CONNECTED) {
    Log.info(CR "You're connected to the network: %s" CR, WiFi.localIP().toString().c_str());
  } 


  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("Small-Wine-Frig-0");
  

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);
  Log.info("Attempting to connect to the MQTT broker: %s" CR, broker);

  if(!mqttClient.connect(broker, port)) {
    Log.warning("MQTT connection failed! Error code = %d" CR, mqttClient.connectError());
  } else {
    Log.info(F("You're connected to the MQTT broker!" CR));
  }

  // Update HA
  update_HA();
  Serial.flush();
  esp_deep_sleep_start();
}



//*********************************************************************
//    Loop Function
//*********************************************************************
void loop() {
  // Do nothing here.
}

