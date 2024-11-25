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
//#   Description : Blynk Deep Sleep Verification
//#                   Voltage Level               : 5.10V
//#                   Power Consump Active State  : 0.400W
//#                   Power Consump Deep Sleep    : 0.000001W (~7uA)
//#
//#
//#
//#       Time struct information:
//#
//#             %A	Full weekday name
//#             %B	Full month name
//#             %d	Day of the month
//#             %Y	Year
//#             %H	Hour in 24h format
//#             %I	Hour in 12h format
//#             %M	Minute
//#             %S	Second
//#           
//#             timeinfo.tm_sec	    int	seconds after the minute	0-61*
//#             timeinfo.tm_min	    int	minutes after the hour	0-59
//#             timeinfo.tm_hour	  int	hours since midnight	0-23
//#             timeinfo.tm_mday	  int	day of the month	1-31
//#             timeinfo.tm_mon	    int	months since January	0-11
//#             timeinfo.tm_year	  int	years since 1900	
//#             timeinfo.tm_wday	  int	days since Sunday	0-6
//#             timeinfo.tm_yday	  int	days since January 1	0-365
//#             timeinfo.tm_isdst	  int	Daylight Saving Time flag	
//#           
//#
//#   Version History:
//#   
//#       Date        Description
//#     -----------   -----------------------------------------------------------------------
//#      2024-09-05    Original Creation
//#      2024-09-07    Added time stamp to Blynk App using NTP
//#      2024-09-14    Added deep sleep
//#      2024-11-11    Added HA MQTT and Battery Sensor
//#
//###########################################################################################
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoMqttClient.h>
#include "arduino_secrets.h"
#include <BlynkSimpleEsp32.h>
#include "ArduinoLog.h"
#include "time.h"


//---------------------------------------------------------
// Device Parameters
//---------------------------------------------------------
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
const char topicMoist[]   = "planter/moisture";
const char topicBatt[]    = "planter/battery";
uint8_t    MQTT_QoS       = 1; // 0:Best Effort, 1:Received ACK, 2:SND/ACK/RSP/ACK
uint8_t    MQTT_retain    = true; 


//---------------------------------------------------------
//  Constants and MQTT Client
//---------------------------------------------------------
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -8*3600; // -8 hours LA time
const int   daylightOffset_sec = 0;

#define TIME_TO_SLEEP  (10 * 60 * 1000000) // 10 Minutes (60 * 1e-6)
int disable_deepsleep = 1;
BlynkTimer timer;

// Define WiFi
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


//---------------------------------------------------------
//  deepSleep function
//---------------------------------------------------------
void deepSleep() {
  Log.info("Going to sleep now" CR);
  delay(1000);
  Serial.flush(); 

  esp_deep_sleep_start();
}


//---------------------------------------------------------
//  Setup WiFi Module
//---------------------------------------------------------
void setupWifi() {
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Log.info(".");
    delay(1000);

    if ((++retry % 16) == 0) {
      Log.info("WiFi Failed to Connect" CR);
      delay(1000);
      //deepSleep();
    }
  }

  Log.info("WiFi Connected : %s" CR, WiFi.localIP().toString());
}


//---------------------------------------------------------
//  Check the Blynk status and update hw/App
//---------------------------------------------------------
void checkBlynkStatus() {

  // Get local time
  char locTime[20];
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Log.fatal("Failed to obtain time" CR);
    Blynk.virtualWrite(V2, "N/A");
  } else {
    sprintf(locTime, "%02d/%02d/%02d %02d:%02d:%02d",\
        timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_year+1900,\
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    Log.info("%s" CR, locTime);
    Blynk.virtualWrite(V2, locTime);
  }

  // update moisture sensor reading
  uint32_t moisture_level = 0;
  for (int i = 0; i<16; i++) {
    moisture_level = moisture_level + analogReadMilliVolts(A0);
  }
  //float moist_lvl_float = moisture_level / 16.0;
  float moist_lvl_float = ((3200.0 - (moisture_level / 16.0)) / 1600.0) * 100.0;
  Blynk.virtualWrite(V1, moist_lvl_float);
  Log.info("Moisture Sensor Reading : %F" CR, moist_lvl_float);

  // Send Mositure to HA-MQTT
  mqttClient.beginMessage(topicMoist, MQTT_retain, MQTT_QoS, false);
  mqttClient.print(moist_lvl_float);
  mqttClient.endMessage();

  // Send Battery Level HA-MQTT
  uint32_t batt_level = 0;
  for (int i = 0; i<16; i++) {
    batt_level = batt_level + analogReadMilliVolts(A2);
  }
  float batt_lvl_float = ((batt_level / 16.0 / 2100.0)) * 100.0;
  //float batt_lvl_float = batt_level / 16.0;
  Log.info("Battery Sensor Reading : %F" CR, batt_lvl_float);
  Log.info("Battery Sensor Raw Reading : %d" CR, analogReadMilliVolts(A2));
  mqttClient.beginMessage(topicBatt, MQTT_retain, MQTT_QoS, false);
  mqttClient.print(batt_lvl_float);
  mqttClient.endMessage();

  // Check if deepSleep is enabled
  if (disable_deepsleep == 0) {
    Log.info("Entering Deep Sleep" CR);
    delay(1000);
    deepSleep();
  } else {
    Log.info("Disabled Deep Sleep" CR);
  }

}

//---------------------------------------------------------
//  Read Blynk disable_deepsleep value
//---------------------------------------------------------
BLYNK_WRITE(V0)
{
  disable_deepsleep = param.asInt();
  Log.info("Deepsleep Status: %d" CR, !disable_deepsleep);
}


//---------------------------------------------------------
//  Print status and sync Blynk VirtualPins
//---------------------------------------------------------
BLYNK_CONNECTED() {
  Log.info("Connected to blynk" CR);
  Blynk.syncAll();
}



//---------------------------------------------------------
//  Setup function
//---------------------------------------------------------
void setup() {
  // Init system
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);
  Log.info("Connected to Serial Port" CR);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP);
    
  setupWifi();
 
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("Planter-0");

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);
  Log.info("Attempting to connect to the MQTT broker: %s" CR, broker);

  while(!mqttClient.connect(broker, port)) {
    Log.warning("MQTT connection failed! Error code = %s" CR, mqttClient.connectError());
    delay(1000);
  }
  Log.info(F("You're connected to the MQTT broker!" CR));

 
  // Configure Blynk
  Log.info("Configure Blynk with AUTH... " CR);
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Blynk.connect() will wait for 30 seconds to connected
  // and return false if device doesn't connect 
  Log.info("Connecting to Blynk..." CR);
  uint8_t retry = 0;
  while (Blynk.connect() == false) {
    Log.info("x");
     
    if ((++retry % 16) == 0) {
      Log.fatal("FAILED connect to Blynk." CR);
      delay(1000);
      //deepSleep();
    }
  }
  
  Log.info("Execute Blynk_run()..." CR);
  Blynk.run();
 
  // Update Blynk display with Moisture Sensor
  Log.info("Blynk syncAll and update Moisture Sensor to App" CR);
  Blynk.syncAll();
  checkBlynkStatus();


 
  // Check if deepSleep is enabled
  if (disable_deepsleep == 0) {
    Log.info("Enabled Deep Sleep" CR);
    delay(1000);
    deepSleep();
  } else {
    Log.info("Disabled Deep Sleep" CR);
    timer.setInterval(2000L, checkBlynkStatus); // check if Blynk server is connected every 2 seconds
  }

}



//---------------------------------------------------------
//  Enter loop if disabled deep sleep
//---------------------------------------------------------
void loop() {
  Blynk.run();
  timer.run();
}
