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
//#   Generic XIAO ESP32 + SHT31 Battery Sensor for Home Assistant
//#   (MIT License, based on odelay.io Small Wine Frig sensor)
//#
//#   Flash this SAME firmware to any number of boards -- no per-device edits needed.
//#
//#   How it works:
//#     * Device ID is derived from the WiFi MAC (e.g. "xiao-a1b2c3"), so every
//#       board is automatically unique.
//#     * On the first boot after power-on, the device publishes Home Assistant
//#       MQTT Discovery configs (retained). HA auto-creates a Device with
//#       Temperature / Humidity / Battery entities. No configuration.yaml edits.
//#     * On every wake, it publishes one retained JSON state message, then
//#       deep sleeps.
//#
//#   To deploy a new sensor: solder battery, flash, power on. Done.
//#   To rename it: HA UI -> Settings -> Devices -> rename (survives re-flash,
//#   since the unique_id stays the same).
//#
//#   Requires: MQTT integration configured in Home Assistant with discovery
//#   enabled (it is by default, prefix "homeassistant").
//#
//#   Version History:
//#       2026-07-06   Generic multi-device version w/ MQTT Discovery
//#
//#############################################################################################

#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <math.h>
#include "arduino_secrets.h"
#include "Wire.h"
#include "SHT31.h"
#include "ArduinoLog.h"
#include "esp_mac.h"


//*********************************************************************
//    System Parameters (identical for every device)
//*********************************************************************

#define UPDATE_RATE_SEC     10 // seconds between sensor checks
#define uS_TO_SEC_FACTOR    1000000ULL

// Only connect WiFi and publish when the temperature changes enough or the
// heartbeat is due. Values survive deep sleep in RTC memory.
#define TEMP_DELTA_F        0.5f  // publish temperature if it moved this much
#define HEARTBEAT_UPDATES   6     // publish at least once every N checks

// Ground Pin D3 and reboot to stop deep sleep (for re-flashing).
// Holding it LOW also forces discovery configs to be re-published.
#define MONITOR_PIN D3 // GPIO21

#define WIFI_FAST_TIMEOUT_MS    5000
#define WIFI_FULL_TIMEOUT_MS    20000

//#define LOG_LEVEL LOG_LEVEL_VERBOSE
#define LOG_LEVEL LOG_LEVEL_SILENT

// MQTT Broker
const char broker[]  = "nuc-sdr";
int        port      = 1883;
uint8_t    MQTT_QoS  = 0;

// HA MQTT Discovery prefix (HA default is "homeassistant")
const char DISCOVERY_PREFIX[] = "homeassistant";

// Report temperature in Fahrenheit? (false = Celsius)
#define USE_FAHRENHEIT true

// Entities expire in HA if no update within this window (seconds).
// Allow one missed heartbeat while still detecting a genuinely offline device.
#define EXPIRE_AFTER_SEC (UPDATE_RATE_SEC * HEARTBEAT_UPDATES * 2)

char ssid[]      = SECRET_SSID;
char pass[]      = SECRET_PASS;
char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;

#define SHT31_ADDRESS   0x44
SHT31 sht;

//*********************************************************************
//    Globals
//*********************************************************************

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// Filled in at boot from the MAC address -- unique per board, stable forever
char deviceId[24];     // e.g. "xiao-a1b2c3"
char stateTopic[64];   // e.g. "sensors/xiao-a1b2c3/state"

// RTC memory survives deep sleep and avoids reconnecting or republishing when
// the temperature is unchanged. These values are cleared on power-on/reset.
RTC_DATA_ATTR bool    rtcWifiValid        = false;
RTC_DATA_ATTR uint8_t rtcBssid[6];
RTC_DATA_ATTR int32_t rtcChannel          = 0;
RTC_DATA_ATTR bool    rtcDiscoverySent    = false;
RTC_DATA_ATTR float   rtcLastTemp        = NAN;
RTC_DATA_ATTR uint8_t rtcUpdatesSinceReport = HEARTBEAT_UPDATES;


//*********************************************************************
//    Build device identity from MAC (no per-device code edits!)
//*********************************************************************
void build_device_identity() {
  uint8_t mac[6];
  // Read from eFuse: valid before WiFi starts, unlike WiFi.macAddress()
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(deviceId, sizeof(deviceId), "xiao-%02x%02x%02x", mac[3], mac[4], mac[5]);
  snprintf(stateTopic, sizeof(stateTopic), "sensors/%s/state", deviceId);
  Log.info("Device ID: %s" CR, deviceId);
}



//*********************************************************************
//    Picewise Lookup Table to calculate battery percent
//*********************************************************************

// Calibration table: voltage (mV) -> percent. MUST be sorted ascending by mV.
// Derived from a measured 750 mAh 1S LiPo full discharge (see header).
struct SocPoint { uint16_t mV; uint8_t pct; };

// Battery Percent Lookup Table
const SocPoint SOC_TABLE[] = {
  {2700,   0}, {3325,   5}, {3395,  10}, {3457,  15}, {3524,  20},
  {3565,  25}, {3625,  30}, {3695,  35}, {3744,  40}, {3771,  45},
  {3795,  50}, {3825,  55}, {3855,  60}, {3894,  65}, {3945,  70},
  {3965,  75}, {3976,  80}, {3986,  85}, {4003,  90}, {4025,  95},
  {4120, 100}
};
const uint8_t SOC_TABLE_LEN = sizeof(SOC_TABLE) / sizeof(SOC_TABLE[0]);

// Return battery percentage (0-100) for a given cell voltage in VOLTS.
// Uses linear interpolation between the two nearest table points.
float batteryPercent(float volts) {
  uint16_t mV = (uint16_t)(volts * 1000.0f + 0.5f);   // volts -> millivolts

  // Clamp to the ends of the table.
  if (mV <= SOC_TABLE[0].mV)                return 0.0f;
  if (mV >= SOC_TABLE[SOC_TABLE_LEN - 1].mV) return 100.0f;

  // Find the segment [i, i+1] that contains mV, then interpolate.
  for (uint8_t i = 0; i < SOC_TABLE_LEN - 1; i++) {
    const SocPoint &lo = SOC_TABLE[i];
    const SocPoint &hi = SOC_TABLE[i + 1];
    if (mV >= lo.mV && mV <= hi.mV) {
      float frac = (float)(mV - lo.mV) / (float)(hi.mV - lo.mV);
      return lo.pct + frac * (hi.pct - lo.pct);
    }
  }
  return 0.0f;  // unreachable
}






//*********************************************************************
//    Sleep (single exit point)
//*********************************************************************
void go_to_sleep() {
  // Close the network interfaces before deep sleep so the next wake starts
  // from a known state and does not leave Wi-Fi power enabled.
  mqttClient.stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#if LOG_LEVEL > LOG_LEVEL_SILENT
  Serial.flush();
#endif
  esp_sleep_enable_timer_wakeup(UPDATE_RATE_SEC * uS_TO_SEC_FACTOR);
  esp_deep_sleep_start();
}


//*********************************************************************
//    WiFi connect with RTC-cached fast reconnect
//*********************************************************************
bool wifi_connect() {
  // Start Wi-Fi in station mode. Wi-Fi sleep is disabled because this device
  // connects, publishes its MQTT state, and immediately enters deep sleep.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Keep credentials and connection state in RTC-backed application state
  // rather than allowing the Wi-Fi library to write flash on every wake.
  WiFi.persistent(false);

  unsigned long start;

  // Reuse the last access point and channel when available. This avoids a
  // channel scan during normal wake cycles and reduces connection time.
  if (rtcWifiValid) {
    Log.info(CR "Fast WiFi reconnect (ch %d)..." CR, rtcChannel);
    WiFi.begin(ssid, pass, rtcChannel, rtcBssid, true);
    start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_FAST_TIMEOUT_MS) {
      delay(50);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Log.warning("Fast reconnect failed, full scan..." CR);
      rtcWifiValid = false;
      WiFi.disconnect(true);
      delay(100);
    }
  }

  // The cached BSSID/channel can become invalid after an access-point change
  // or an intermittent failure, so retry with a normal SSID scan.
  if (WiFi.status() != WL_CONNECTED) {
    Log.info(CR "Connecting to SSID: %s" CR, ssid);
    WiFi.begin(ssid, pass);
    start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_FULL_TIMEOUT_MS) {
      delay(100);
    }
  }

  // Save the successful access point details for the next deep-sleep wake.
  if (WiFi.status() == WL_CONNECTED) {
    memcpy(rtcBssid, WiFi.BSSID(), 6);
    rtcChannel   = WiFi.channel();
    rtcWifiValid = true;
    Log.info(CR "Connected: %s" CR, WiFi.localIP().toString().c_str());
    return true;
  }

  // Do not reuse stale connection details after both connection attempts fail.
  Log.warning("Not connected to WiFi!" CR);
  rtcWifiValid = false;
  return false;
}


//*********************************************************************
//    HA MQTT Discovery
//    Publishes one retained config per entity. HA creates/updates the
//    device automatically. Sent only on power-on boot (retained on the
//    broker afterward), so wakeup cycles pay zero extra radio time.
//*********************************************************************
void publish_discovery_entity(const char* key,        // JSON key in state payload
                              const char* niceName,   // entity name shown in HA
                              const char* devClass,   // HA device_class
                              const char* unit,
                              bool diagnostic = false,  // true => shown in HA "Diagnostic" section
                              int8_t precision = -1) { // decimal places HA should display (-1 = HA default)
  char cfgTopic[96];
  snprintf(cfgTopic, sizeof(cfgTopic), "%s/sensor/%s-%s/config",
           DISCOVERY_PREFIX, deviceId, key);

  char precisionField[24] = "";
  if (precision >= 0) {
    snprintf(precisionField, sizeof(precisionField), "\"sug_dsp_prc\":%d,", precision);
  }

  char payload[512];
  snprintf(payload, sizeof(payload),
    "{"
      "\"name\":\"%s\","
      "\"uniq_id\":\"%s-%s\","
      "\"stat_t\":\"%s\","
      "\"val_tpl\":\"{{ value_json.%s }}\","
      "\"dev_cla\":\"%s\","
      "\"unit_of_meas\":\"%s\","
      "\"stat_cla\":\"measurement\","
      "%s"
      "%s"
      "\"exp_aft\":%d,"
      "\"dev\":{"
        "\"ids\":[\"%s\"],"
        "\"name\":\"XIAO Sensor %s\","
        "\"mf\":\"odelay.io\","
        "\"mdl\":\"XIAO ESP32 + SHT31\""
      "}"
    "}",
    niceName, deviceId, key, stateTopic, key, devClass, unit,
    diagnostic ? "\"ent_cat\":\"diagnostic\"," : "",
    precisionField,
    EXPIRE_AFTER_SEC, deviceId, deviceId);

  // retain=true so HA re-reads configs after its own restarts.
  // Size MUST be passed explicitly: the unsized beginMessage() overload
  // buffers via a 256-byte internal buffer and silently truncates.
  mqttClient.beginMessage(cfgTopic, strlen(payload), true, MQTT_QoS, false);
  mqttClient.print(payload);
  mqttClient.endMessage();
}

void publish_discovery() {
  Log.info("Publishing HA discovery configs..." CR);
  publish_discovery_entity("temperature", "Temperature", "temperature",
                           USE_FAHRENHEIT ? "\u00b0F" : "\u00b0C");
  publish_discovery_entity("humidity",    "Humidity",    "humidity",  "%");
  publish_discovery_entity("battery",     "Battery",     "battery",   "%");
  publish_discovery_entity("battery_voltage", "Battery Voltage", "voltage", "V", true, 2);
  publish_discovery_entity("rssi",        "WiFi Signal", "signal_strength", "dBm", true);
  rtcDiscoverySent = true;
}


//*********************************************************************
//    Read sensors + publish one JSON state message
//*********************************************************************
void update_HA(float sht_temp, float sht_humid) {

  // Read battery voltage and calculate percentage.
  // Average 16 ADC reads to reduce noise.
  uint32_t batt_level = 0;
  for (int i = 0; i < 16; i++) {
    batt_level += analogReadMilliVolts(A1); // GPIO1/D1
  }
  float batt_adc_volts = (batt_level / 16) / 1000.0; // volts measured at the ADC pin

  // Battery is monitored through a voltage divider, so double the ADC
  // reading to get the actual battery voltage.
  float batt_voltage  = batt_adc_volts * 2.0;
  float batt_percent = batteryPercent(batt_voltage);

  // WiFi signal strength (dBm). We're already connected, so this is free.
  int8_t rssi = WiFi.RSSI();

  Log.info("T: %F  H: %F  Batt: %F  BattV: %F  RSSI: %d" CR, sht_temp, sht_humid, batt_percent, batt_voltage, rssi);

  char payload[160];
  snprintf(payload, sizeof(payload),
           "{\"temperature\":%.2f,\"humidity\":%.2f,\"battery\":%.2f,\"battery_voltage\":%.2f,\"rssi\":%d}",
           sht_temp, sht_humid, batt_percent, batt_voltage, rssi);

  // retain=true: HA shows the last reading immediately after its own restart,
  // instead of "unknown" until the next wake cycle
  mqttClient.beginMessage(stateTopic, true, MQTT_QoS, false);
  mqttClient.print(payload);
  mqttClient.endMessage();
  rtcLastTemp = sht_temp;
  rtcUpdatesSinceReport = 0;
}


//*********************************************************************
//    Setup (main)
//*********************************************************************
void setup() {
  // Do not initialize or access the UART when logging is disabled. This saves
  // the serial peripheral setup and keeps silent builds free of serial output.
#if LOG_LEVEL > LOG_LEVEL_SILENT
  Serial.begin(115200);
  delay(1000);  // give the serial port time to start before logging
#endif
  Log.begin(LOG_LEVEL, &Serial);
  Log.info("Booting..." CR);

  pinMode(MONITOR_PIN, INPUT_PULLUP);
  bool forceDiscovery = false;
  while (digitalRead(MONITOR_PIN) == LOW) {
#if LOG_LEVEL > LOG_LEVEL_SILENT
    Serial.println("Stopped HA Sensor boot... (will re-send discovery on release)");
#endif
    forceDiscovery = true;
    delay(1000);
  }

  Wire.begin();
  Wire.setClock(400000);
  sht.begin();
  Log.info("Configured I2C Clock" CR);

  // Read the sensor before starting Wi-Fi. Most wakes can therefore return to
  // deep sleep without powering the Wi-Fi radio or opening an MQTT session.
  bool sht_success = sht.read(false);
  if (!sht_success) {
    Log.fatal(F("FAILED: Unable to Read SHT, trying again..." CR));
    delay(20);
    sht_success = sht.read(false);
  }
  float sht_temp  = USE_FAHRENHEIT ? sht.getFahrenheit() : sht.getTemperature();
  float sht_humid = sht.getHumidity();

  // Publish on the first reading, after the configured temperature change, or
  // at the heartbeat interval so Home Assistant does not expire the entities.
  if (rtcUpdatesSinceReport < 255) rtcUpdatesSinceReport++;
  bool heartbeatDue = rtcUpdatesSinceReport >= HEARTBEAT_UPDATES;
  bool temperatureDue = sht_success &&
                        (heartbeatDue || isnan(rtcLastTemp) ||
                         fabsf(sht_temp - rtcLastTemp) >= TEMP_DELTA_F);
  bool publishDue = !sht_success || temperatureDue || !rtcDiscoverySent || forceDiscovery;

  Log.info("T: %F F  H: %F  shtOk:%d  heartbeat:%d  publish:%d" CR,
           sht_temp, sht_humid, sht_success, heartbeatDue, publishDue);
  if (!publishDue) {
    go_to_sleep();
  }

  // Only the publish path pays the cost of building the device identity,
  // connecting Wi-Fi, and opening the MQTT session.
  build_device_identity();

  if (!wifi_connect()) {
    Log.warning("WiFi connection failed!" CR);
    go_to_sleep();
  }

  // MQTT client ID is derived from the MAC address, 
  // so every board is unique.h
  mqttClient.setId(deviceId);   // MAC-derived => always unique, no collisions
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);

  if (!mqttClient.connect(broker, port)) {
    Log.warning("MQTT connection failed! Error code = %d" CR, mqttClient.connectError());
    go_to_sleep();
  }

  // First boot after power-on (or forced): register with Home Assistant
  if (!rtcDiscoverySent || forceDiscovery) {
    publish_discovery();
  }

  if (sht_success) {
    update_HA(sht_temp, sht_humid);
  }
  go_to_sleep();
}

void loop() {
  // never reached
}

