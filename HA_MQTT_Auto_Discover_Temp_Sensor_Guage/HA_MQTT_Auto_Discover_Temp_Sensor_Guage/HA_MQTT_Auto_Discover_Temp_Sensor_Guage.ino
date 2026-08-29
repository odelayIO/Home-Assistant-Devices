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
//#   Generic XIAO ESP32 + SHT31 + MAX17048 Battery Sensor for Home Assistant
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
//#   Libraries:
//#       ArduinoMqttClient, ArduinoLog, SHT31 (Rob Tillaart),
//#       Adafruit MAX1704X  (Library Manager -> "Adafruit MAX1704X")
//#       https://github.com/adafruit/Adafruit_MAX1704X
//#
//#   Hardware (battery monitoring):
//#       The resistor-divider / ADC battery measurement on A1 (GPIO1/D1) is gone.
//#       Battery state now comes from an Adafruit MAX17048 breakout on the same
//#       I2C bus as the SHT31 (gauge = 0x36, SHT31 = 0x44, no conflict):
//#
//#           MAX17048 VIN  -> XIAO 3V3
//#           MAX17048 GND  -> XIAO GND
//#           MAX17048 SDA  -> XIAO SDA (D4)
//#           MAX17048 SCL  -> XIAO SCL (D5)
//#           LiPo JST      -> either MAX17048 JST port
//#           2nd JST port  -> XIAO battery input (the two ports are in parallel)
//#
//#       The MAX17048 stays powered from the cell through deep sleep, so its
//#       ModelGauge state-of-charge estimate keeps tracking between wakes -- no
//#       voltage lookup table and no divider calibration needed. D1/A1 is now free.
//#
//#   Version History:
//#       2026-07-06   Generic multi-device version w/ MQTT Discovery
//#       2026-08-17   Replaced ADC + SoC lookup table with MAX17048 fuel gauge;
//#                    added battery charge/discharge rate entity; sensor values
//#                    are published as JSON null when a read fails so HA keeps
//#                    the last good state instead of showing garbage.
//#
//#############################################################################################

#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "arduino_secrets.h"
#include "Wire.h"
#include "SHT31.h"
#include "Adafruit_MAX1704X.h"
#include "ArduinoLog.h"
#include "esp_mac.h"
#include <math.h>


//*********************************************************************
//    System Parameters (identical for every device)
//*********************************************************************

#define UPDATE_RATE_SEC     10
#define uS_TO_SEC_FACTOR    1000000ULL

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
// 3x the sleep interval tolerates a couple of missed cycles.
#define EXPIRE_AFTER_SEC (UPDATE_RATE_SEC * 3)

// Force the fuel gauge into hibernate mode before deep sleep (23uA -> ~3uA).
// The MAX17048 already hibernates on its own once the charge rate drops below
// its hibernation threshold (default 5%/hr), which a sleeping sensor always
// does, so leave this false unless you want the gauge parked immediately.
// Forcing it costs a little SoC-tracking resolution right after each wake.
#define MAX17048_FORCE_HIBERNATE  false

char ssid[]      = SECRET_SSID;
char pass[]      = SECRET_PASS;
char mqtt_user[] = SECRET_MQTT_USER;
char mqtt_pass[] = SECRET_MQTT_PASS;

#define SHT31_ADDRESS   0x44
SHT31 sht;

// MAX17048 LiPoly / LiIon fuel gauge (I2C 0x36, fixed address)
Adafruit_MAX17048 maxlipo;
bool gaugeOK = false;

//*********************************************************************
//    Globals
//*********************************************************************

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// Filled in at boot from the MAC address -- unique per board, stable forever
char deviceId[24];     // e.g. "xiao-a1b2c3"
char stateTopic[64];   // e.g. "sensors/xiao-a1b2c3/state"

// RTC memory: survives deep sleep, cleared on power-on
RTC_DATA_ATTR bool    rtcWifiValid        = false;
RTC_DATA_ATTR uint8_t rtcBssid[6];
RTC_DATA_ATTR int32_t rtcChannel          = 0;
RTC_DATA_ATTR bool    rtcDiscoverySent    = false;


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
//    Fuel gauge init
//    Never block forever waiting for the gauge (unlike the Adafruit
//    example) -- this runs on battery, so one retry then move on.
//*********************************************************************
void fuel_gauge_begin() {
  gaugeOK = maxlipo.begin(&Wire);
  if (!gaugeOK) {
    delay(50);
    gaugeOK = maxlipo.begin(&Wire);
  }

  if (!gaugeOK) {
    Log.error("MAX17048 not found! Battery values will be published as null." CR);
    return;
  }

  Log.info("MAX17048 found, chip ID 0x%x" CR, maxlipo.getChipID());

  if (MAX17048_FORCE_HIBERNATE) {
    maxlipo.wake();   // gauge was parked before the last deep sleep
  }

  // Clear the power-on reset indicator so it doesn't stay latched.
  // Note: no quickStart() here -- Adafruit warns it resets the charge
  // calculator, and the gauge self-calibrates within a few cycles anyway.
  if (maxlipo.isActiveAlert()) {
    uint8_t flags = maxlipo.getAlertStatus();
    if (flags & MAX1704X_ALERTFLAG_RESET_INDICATOR) {
      maxlipo.clearAlertFlag(MAX1704X_ALERTFLAG_RESET_INDICATOR);
      Log.info("Fuel gauge power-on reset detected (fresh battery?)" CR);
    }
  }
}


//*********************************************************************
//    JSON number formatting: NaN becomes null so HA ignores the value
//    instead of charting a bogus reading.
//*********************************************************************
void fmt_json_float(char* buf, size_t len, float v, uint8_t decimals) {
  if (isnan(v)) {
    snprintf(buf, len, "null");
  } else {
    snprintf(buf, len, "%.*f", (int)decimals, v);
  }
}


//*********************************************************************
//    Sleep (single exit point)
//*********************************************************************
void go_to_sleep() {
  if (MAX17048_FORCE_HIBERNATE && gaugeOK) {
    maxlipo.hibernate();
  }
  mqttClient.stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(UPDATE_RATE_SEC * uS_TO_SEC_FACTOR);
  esp_deep_sleep_start();
}


//*********************************************************************
//    WiFi connect with RTC-cached fast reconnect
//*********************************************************************
bool wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.persistent(false);

  unsigned long start;

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

  if (WiFi.status() != WL_CONNECTED) {
    Log.info(CR "Connecting to SSID: %s" CR, ssid);
    WiFi.begin(ssid, pass);
    start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_FULL_TIMEOUT_MS) {
      delay(100);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    memcpy(rtcBssid, WiFi.BSSID(), 6);
    rtcChannel   = WiFi.channel();
    rtcWifiValid = true;
    Log.info(CR "Connected: %s" CR, WiFi.localIP().toString().c_str());
    return true;
  }

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
                              const char* devClass,   // HA device_class (NULL/"" = none)
                              const char* unit,
                              bool diagnostic = false,  // true => shown in HA "Diagnostic" section
                              int8_t precision = -1) { // decimal places HA should display (-1 = HA default)
  char cfgTopic[96];
  snprintf(cfgTopic, sizeof(cfgTopic), "%s/sensor/%s-%s/config",
           DISCOVERY_PREFIX, deviceId, key);

  // device_class is optional: charge rate (%/hr) has no matching HA class
  char devClassField[48] = "";
  if (devClass != NULL && devClass[0] != '\0') {
    snprintf(devClassField, sizeof(devClassField), "\"dev_cla\":\"%s\",", devClass);
  }

  char precisionField[24] = "";
  if (precision >= 0) {
    snprintf(precisionField, sizeof(precisionField), "\"sug_dsp_prc\":%d,", precision);
  }

  // The value template renders to an empty string when the JSON value is
  // null, which HA treats as "ignore this update" and keeps the last state.
  // (Testing "is not none" rather than filtering on truthiness keeps a
  // legitimate reading of 0 from being thrown away.)
  char payload[640];
  snprintf(payload, sizeof(payload),
    "{"
      "\"name\":\"%s\","
      "\"uniq_id\":\"%s-%s\","
      "\"stat_t\":\"%s\","
      "\"val_tpl\":\"{{ value_json.%s if value_json.%s is not none else '' }}\","
      "%s"
      "\"unit_of_meas\":\"%s\","
      "\"stat_cla\":\"measurement\","
      "%s"
      "%s"
      "\"exp_aft\":%d,"
      "\"dev\":{"
        "\"ids\":[\"%s\"],"
        "\"name\":\"XIAO Sensor %s\","
        "\"mf\":\"odelay.io\","
        "\"mdl\":\"XIAO ESP32 + SHT31 + MAX17048\""
      "}"
    "}",
    niceName, deviceId, key, stateTopic, key, key,
    devClassField, unit,
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
  publish_discovery_entity("battery",     "Battery",     "battery",   "%", false, 1);
  publish_discovery_entity("battery_voltage", "Battery Voltage", "voltage", "V", true, 3);
  // No HA device_class fits %/hr, so pass NULL and let it be a plain number.
  // Positive = charging, negative = discharging.
  publish_discovery_entity("battery_rate", "Battery Charge Rate", NULL, "%/h", true, 1);
  publish_discovery_entity("rssi",        "WiFi Signal", "signal_strength", "dBm", true);
  rtcDiscoverySent = true;
}


//*********************************************************************
//    Read sensors + publish one JSON state message
//*********************************************************************
void update_HA() {
  //----- SHT31 temperature / humidity -----
  bool sht_success = sht.read(false);
  if (sht_success == false) {
    Log.fatal(F("FAILED: Unable to Read SHT, trying again..." CR));
    delay(20);
    sht_success = sht.read(false);
  }

  float sht_temp  = NAN;
  float sht_humid = NAN;
  if (sht_success) {
    sht_temp  = USE_FAHRENHEIT ? sht.getFahrenheit() : sht.getTemperature();
    sht_humid = sht.getHumidity();
  }

  //----- MAX17048 fuel gauge -----
  // cellVoltage() returns NaN if the gauge can't be read (e.g. no cell
  // attached), so it gates the other two reads.
  float batt_voltage = NAN;
  float batt_percent = NAN;
  float batt_rate    = NAN;

  if (gaugeOK) {
    batt_voltage = maxlipo.cellVoltage();
    if (!isnan(batt_voltage)) {
      batt_percent = maxlipo.cellPercent();
      // The gauge can read slightly outside 0-100 while charging or nearly
      // empty; HA's battery device_class expects a clean percentage.
      if (batt_percent > 100.0f) batt_percent = 100.0f;
      if (batt_percent <   0.0f) batt_percent =   0.0f;
      batt_rate = maxlipo.chargeRate();   // %/hr, negative while discharging
    } else {
      Log.warning("Fuel gauge read failed -- battery disconnected?" CR);
    }
  }

  // WiFi signal strength (dBm). We're already connected, so this is free.
  int8_t rssi = WiFi.RSSI();

  Log.info("T: %F  H: %F  Batt: %F %%  BattV: %F  Rate: %F %%/hr  RSSI: %d" CR,
           sht_temp, sht_humid, batt_percent, batt_voltage, batt_rate, rssi);

  char sTemp[12], sHumid[12], sPct[12], sVolt[12], sRate[12];
  fmt_json_float(sTemp,  sizeof(sTemp),  sht_temp,     2);
  fmt_json_float(sHumid, sizeof(sHumid), sht_humid,    2);
  fmt_json_float(sPct,   sizeof(sPct),   batt_percent, 2);
  fmt_json_float(sVolt,  sizeof(sVolt),  batt_voltage, 3);
  fmt_json_float(sRate,  sizeof(sRate),  batt_rate,    2);

  char payload[224];
  snprintf(payload, sizeof(payload),
           "{\"temperature\":%s,\"humidity\":%s,\"battery\":%s,"
           "\"battery_voltage\":%s,\"battery_rate\":%s,\"rssi\":%d}",
           sTemp, sHumid, sPct, sVolt, sRate, rssi);

  // retain=true: HA shows the last reading immediately after its own restart,
  // instead of "unknown" until the next wake cycle
  mqttClient.beginMessage(stateTopic, strlen(payload), true, MQTT_QoS, false);
  mqttClient.print(payload);
  mqttClient.endMessage();
}


//*********************************************************************
//    Setup (main)
//*********************************************************************
void setup() {
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);
  Log.info("Booting..." CR);

  pinMode(MONITOR_PIN, INPUT_PULLUP);
  bool forceDiscovery = false;
  while (digitalRead(MONITOR_PIN) == LOW) {
    Serial.println("Stopped HA Sensor boot... (will re-send discovery on release)");
    forceDiscovery = true;
    delay(1000);
  }

  Wire.begin();
  Wire.setClock(400000);   // MAX17048 and SHT31 both handle 400 kHz
  sht.begin();
  fuel_gauge_begin();
  Log.info("Configured I2C Clock" CR);

  build_device_identity();

  if (!wifi_connect()) {
    Log.warning("WiFi connection failed!" CR);
    go_to_sleep();
  }

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

  update_HA();
  go_to_sleep();
}

void loop() {
  // never reached
}
