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
//#   XIAO ESP32-C6 + SHT31 Battery Sensor for Home Assistant -- native Zigbee
//#   (MIT License, based on odelay.io Small Wine Frig sensor)
//#
//#   Flash this SAME firmware to any number of boards -- no per-device edits needed.
//#
//#   How it works:
//#     * The device is a Zigbee End Device (sleepy). It joins the Zigbee network
//#       of whatever coordinator is running (HA ZHA add-in radio, Zigbee2MQTT
//#       coordinator, etc.) -- there is no WiFi, no MQTT broker, no secrets file.
//#     * Identity/pairing is handled entirely by the Zigbee stack (IEEE address +
//#       network keys stored in NVS), so every board is automatically unique.
//#     * Exposes one endpoint with the standard ZCL Temperature Measurement,
//#       Relative Humidity Measurement, and Power Configuration (battery)
//#       clusters. Home Assistant (via ZHA or Zigbee2MQTT) auto-creates the
//#       Temperature / Humidity / Battery entities -- no configuration.yaml
//#       edits, no MQTT discovery payloads to craft.
//#     * On every wake, it reads the sensor, reports the three attributes, and
//#       deep sleeps.
//#
//#   To deploy a new sensor: solder battery, flash, power on, then open the
//#   coordinator's network for joining (Zigbee.setRebootOpenNetwork() below
//#   handles this automatically after a flash/power-on). Done.
//#
//#   Ground MONITOR_PIN and reboot to halt before joining (for re-flashing over
//#   serial). Releasing it factory-resets the Zigbee stack, forcing a full
//#   rejoin -- use this if the coordinator/network changed.
//#
//#   Note: the ZCL Temperature Measurement cluster is always reported in
//#   Celsius (per spec); Home Assistant converts to the display unit
//#   configured in the user's profile, so there is no Fahrenheit toggle here.
//#
//#   Requires: Arduino-ESP32 core with Tools -> Zigbee mode: "Zigbee ED (end
//#   device)" and Tools -> Partition Scheme: "Zigbee 4MB with spiffs" selected.
//#   Target board: Seeed XIAO ESP32-C6 (802.15.4 radio required for Zigbee --
//#   the plain ESP32-C3/S3 XIAO boards do NOT have this radio).
//#
//#   Version History:
//#       2026-07-06   Generic multi-device version w/ MQTT Discovery
//#       2026-08-27   Converted from WiFi/MQTT to native Zigbee (XIAO ESP32-C6)
//#
//#############################################################################################

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include "Wire.h"
#include "SHT31.h"
#include "ArduinoLog.h"


//*********************************************************************
//    System Parameters (identical for every device)
//*********************************************************************

#define TEMP_SENSOR_ENDPOINT_NUMBER 10

#define TIME_TO_SLEEP_SEC       60    // seconds
#define uS_TO_SEC_FACTOR        1000000ULL
#define ZIGBEE_JOIN_TIMEOUT_MS  10000
#define REPORT_TIMEOUT_MS       1000
#define REPORT_MAX_TRIES        3

// Ground Pin D3 and reboot to stop before joining (for re-flashing).
// Holding it LOW also factory-resets the Zigbee stack, forcing a full rejoin.
#define MONITOR_PIN D3 // GPIO21

//#define LOG_LEVEL LOG_LEVEL_VERBOSE
#define LOG_LEVEL LOG_LEVEL_SILENT

#define SHT31_ADDRESS   0x44
SHT31 sht;

//*********************************************************************
//    Globals
//*********************************************************************

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);

// Number of attributes still awaiting a confirmed report this wake cycle
// (temperature + humidity + battery percentage).
volatile uint8_t dataToSend = 3;
volatile bool     resend    = false;


//*********************************************************************
//    Picewise Lookup Table to calculate battery percent
//*********************************************************************

// Calibration table: voltage (mV) -> percent. MUST be sorted ascending by mV.
struct SocPoint { uint16_t mV; uint8_t pct; };

//  //--------------------------
//  // 750mAh Battery Table
//  //--------------------------
//  const SocPoint SOC_TABLE[] = {
//    {2745,   0}, {3310,   5}, {3380,  10}, {3430,  15}, {3480,  20},
//    {3520,  25}, {3560,  30}, {3610,  35}, {3660,  40}, {3700,  45},
//    {3740,  50}, {3760,  55}, {3800,  60}, {3830,  65}, {3870,  70},
//    {3910,  75}, {3930,  80}, {3940,  85}, {3950,  90}, {3990,  95},
//    {4040, 100}
//  };

//--------------------------
// 1100mAh Battery Table
//--------------------------
const SocPoint SOC_TABLE[] = {
  {2700,   0}, {3325,   5}, {3395,  10}, {3457,  15}, {3524,  20},
  {3565,  25}, {3625,  30}, {3695,  35}, {3744,  40}, {3771,  45},
  {3795,  50}, {3825,  55}, {3855,  60}, {3894,  65}, {3945,  70},
  {3965,  75}, {3976,  80}, {3986,  85}, {4003,  90}, {4025,  95},
  {4120, 100}
};
const uint8_t SOC_TABLE_LEN = sizeof(SOC_TABLE) / sizeof(SOC_TABLE[0]);

/*
 * Return battery percentage (0-100) for a given cell voltage in VOLTS.
 * Uses linear interpolation between the two nearest table points.
 */
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
//    Zigbee report-confirmation callback
//*********************************************************************
void onGlobalResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status, uint8_t endpoint, uint16_t cluster) {
  Log.trace("Zigbee response cmd: %d, status: %s, ep: %u, cluster: 0x%04x" CR,
            command, esp_zb_zcl_status_to_name(status), endpoint, cluster);
  if ((command == ZB_CMD_REPORT_ATTRIBUTE) && (endpoint == TEMP_SENSOR_ENDPOINT_NUMBER)) {
    switch (status) {
      case ESP_ZB_ZCL_STATUS_SUCCESS: dataToSend--; break;
      case ESP_ZB_ZCL_STATUS_FAIL:    resend = true; break;
      default: break;
    }
  }
}


//*********************************************************************
//    Sleep (single exit point)
//*********************************************************************
void go_to_sleep() {
  Serial.flush();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_SEC * uS_TO_SEC_FACTOR);
  esp_deep_sleep_start();
}


//*********************************************************************
//    Read sensors, update the Zigbee attributes, and report them.
//    Retries on failure/timeout, then sleeps either way (single exit
//    point via go_to_sleep(), same as the previous WiFi/MQTT version).
//*********************************************************************
void update_HA() {
  bool sht_success = sht.read(false);
  if (sht_success == false) {
    Log.fatal(F("FAILED: Unable to Read SHT, trying again..." CR));
    delay(20);
    sht_success = sht.read(false);
  }
  float sht_temp  = sht.getTemperature();  // ZCL Temperature Measurement cluster is always Celsius
  float sht_humid = sht.getHumidity();

  uint32_t batt_level = 0;
  for (int i = 0; i < 16; i++) {
    batt_level += analogReadMilliVolts(A1); // GPIO1/D1
  }
  float batt_adc_volts = (batt_level / 16) / 1000.0; // volts measured at the ADC pin
  // Battery is monitored through a voltage divider, so double the ADC
  // reading to get the actual battery voltage.
  float batt_voltage = batt_adc_volts * 2.0;
  float batt_percent = batteryPercent(batt_voltage);

  Log.info("T: %F C  H: %F  Batt: %F%%  BattV: %F" CR, sht_temp, sht_humid, batt_percent, batt_voltage);

  zbTempSensor.setTemperature(sht_temp);
  zbTempSensor.setHumidity(sht_humid);
  zbTempSensor.setBatteryPercentage((uint8_t)(batt_percent + 0.5f));
  zbTempSensor.setBatteryVoltage((uint8_t)(batt_voltage * 10.0f + 0.5f)); // units of 100mV

  dataToSend = 3;
  resend     = false;
  zbTempSensor.report();                  // temperature + humidity
  zbTempSensor.reportBatteryPercentage(); // battery voltage is not a reportable attribute

  unsigned long startTime = millis();
  int tries = 0;
  Log.info("Waiting for report confirmation..." CR);
  while (dataToSend != 0 && tries < REPORT_MAX_TRIES) {
    if (resend) {
      Log.warning("Resending data on failure!" CR);
      resend    = false;
      dataToSend = 3;
      zbTempSensor.report();
      zbTempSensor.reportBatteryPercentage();
    }
    if (millis() - startTime >= REPORT_TIMEOUT_MS) {
      Log.warning("Report timeout! Report again" CR);
      dataToSend = 3;
      zbTempSensor.report();
      zbTempSensor.reportBatteryPercentage();
      startTime = millis();
      tries++;
    }
    delay(50);
  }
}


//*********************************************************************
//    Setup (main)
//*********************************************************************
void setup() {
  Serial.begin(115200);
  Log.begin(LOG_LEVEL, &Serial);
  Log.info("Booting..." CR);

  pinMode(MONITOR_PIN, INPUT_PULLUP);
  bool forceRejoin = false;
  while (digitalRead(MONITOR_PIN) == LOW) {
    Serial.println("Stopped HA Sensor boot... (will factory-reset Zigbee and rejoin on release)");
    forceRejoin = true;
    delay(1000);
  }

  Wire.begin();
  Wire.setClock(400000);
  sht.begin();
  Log.info("Configured I2C Clock" CR);

  if (forceRejoin) {
    // Erase Zigbee NVS (network keys/short address) so the next begin() does
    // a full network scan + join instead of a fast reconnect. false = do not
    // restart yet, continue this same boot after the reset.
    Zigbee.factoryReset(false);
  }

  zbTempSensor.setManufacturerAndModel("odelay.io", "XIAO ESP32-C6 + SHT31");

  // SHT31 range is roughly -40..125C; keep bounds sane for reporting.
  zbTempSensor.setMinMaxValue(-40, 80);
  zbTempSensor.setTolerance(0.5);

  // Power source = battery. Percentage/voltage are placeholders until the
  // first real reading is set and reported in update_HA().
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100, 35);

  zbTempSensor.addHumiditySensor(0, 100, 1, 0.0);

  Zigbee.onGlobalDefaultResponse(onGlobalResponse);
  Zigbee.addEndpoint(&zbTempSensor);

  // Sleepy End Device: keep_alive short to avoid interfering with reporting.
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;

  // Battery powered: fail fast rather than burning power scanning forever.
  Zigbee.setTimeout(ZIGBEE_JOIN_TIMEOUT_MS);

  // Reopen the coordinator's network for a few seconds after every boot, so
  // a freshly-flashed or factory-reset board can always find its way in.
  Zigbee.setRebootOpenNetwork(30);

  if (!Zigbee.begin(&zigbeeConfig, false)) {
    Log.warning("Zigbee failed to start! Rebooting..." CR);
    ESP.restart();
  }

  Log.info("Connecting to Zigbee network..." CR);
  unsigned long joinStart = millis();
  while (!Zigbee.connected() && (millis() - joinStart) < ZIGBEE_JOIN_TIMEOUT_MS) {
    delay(100);
  }

  if (!Zigbee.connected()) {
    Log.warning("Not connected to Zigbee network!" CR);
    go_to_sleep();
  }
  Log.info("Connected to Zigbee network" CR);

  update_HA();
  go_to_sleep();
}

void loop() {
  // never reached
}
