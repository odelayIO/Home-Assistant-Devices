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
//#       * 0 - LOG_LEVEL_SILENT     no output (Serial is not started at all)
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
//#     * The device is a Zigbee End Device. It joins the Zigbee network of
//#       whatever coordinator is running (ZHA, Zigbee2MQTT, etc.).
//#     * Exposes one endpoint with the standard ZCL Temperature Measurement,
//#       Relative Humidity Measurement, and Power Configuration (battery) clusters.
//#     * On every wake it reads the SHT31 FIRST, before touching the radio.
//#       - If temperature/humidity haven't changed enough and no heartbeat is
//#         due, it goes straight back to deep sleep without starting Zigbee.
//#       - Otherwise it starts Zigbee, reports only what is due, waits for the
//#         confirmations, and deep sleeps.
//#     * Battery is measured and reported roughly once an hour.
//#     * If the network can't be reached, sleep time backs off (up to 1 hour)
//#       instead of rebooting in a loop.
//#
//#   To deploy a new sensor: solder battery, flash, enable "Permit join" in
//#   ZHA/Zigbee2MQTT, then power on.
//#
//#   Ground MONITOR_PIN and reboot to halt before joining (for re-flashing over
//#   serial). Releasing it factory-resets the Zigbee stack, forcing a full
//#   rejoin -- use this if the coordinator/network changed.
//#
//#   Note: the ZCL Temperature Measurement cluster is always reported in
//#   Celsius (per spec); Home Assistant converts to the display unit.
//#
//#   Requires: Arduino-ESP32 core with Tools -> Zigbee mode: "Zigbee ED (end
//#   device)" and Tools -> Partition Scheme: "Zigbee 4MB with spiffs" selected.
//#   Also recommended: Tools -> Core Debug Level: "None".
//#
//#   Version History:
//#       2026-07-06   Generic multi-device version w/ MQTT Discovery
//#       2026-08-27   Converted from WiFi/MQTT to native Zigbee (XIAO ESP32-C6)
//#       2026-09-10   Power optimizations: shorter rejoin scan, sensor read before
//#                    radio, report-on-change with heartbeat, hourly battery report,
//#                    per-attribute confirmation, backoff instead of reboot loop
//#       2026-09-10   SHT31 medium-repeatability single shot (7 ms instead of 15 ms),
//#                    soft reset only after power-up/reset
//#
//#############################################################################################

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include <math.h>
#include "Zigbee.h"
#include "Wire.h"
#include "SHT31.h"
#include "ArduinoLog.h"


//*********************************************************************
//    System Parameters (identical for every device)
//*********************************************************************

#define TEMP_SENSOR_ENDPOINT_NUMBER 10

#define TIME_TO_SLEEP_SEC 300  // Sleep duration in sec
#define uS_TO_SEC_FACTOR 1000000ULL

// POWER: report-on-change. A wake with no significant change skips the radio.
#define TEMP_DELTA_C 0.5f     // report temperature if it moved this much
#define HUMID_DELTA_PCT 5.0f  // report humidity if it moved this much
// Force a report at least every N wakes, even with no change (3 x 10 min = 30 min).
// Keep this well under the parent's end-device timeout (library default: 64 min),
// otherwise the parent drops the device from its child table.
#define HEARTBEAT_WAKES 6
// POWER: battery changes slowly -- measure/report roughly hourly (6 x 10 min).
#define BATTERY_REPORT_WAKES 12

// POWER: rejoin scan time per channel. Library default is 3 = (2^3+1) x 15.36 ms
// = 138 ms of receiver-on time every wake (the 140 ms idle gap in the PPK2 capture).
// 1 = 46 ms, 2 = 77 ms. If rejoins start failing, change to 2.
#define ZIGBEE_SCAN_DURATION 3

#define ZIGBEE_BEGIN_TIMEOUT_MS 10000
#define ZIGBEE_JOIN_TIMEOUT_MS 10000
// POWER: confirmations normally arrive within ~150 ms. Don't wait 1 s x 3 tries.
#define REPORT_TIMEOUT_MS 500
#define REPORT_RETRIES 1  // one resend of anything unconfirmed

// POWER: on network failure, double the sleep time each attempt, up to this cap.
#define MAX_BACKOFF_SEC 3600

// Ground Pin D3 and reboot to stop before joining (for re-flashing).
// Holding it LOW also factory-resets the Zigbee stack, forcing a full rejoin.
#define MONITOR_PIN D3  // GPIO21

//#define LOG_LEVEL LOG_LEVEL_VERBOSE
#define LOG_LEVEL LOG_LEVEL_SILENT

#define SHT31_ADDRESS 0x44
SHT31 sht(SHT31_ADDRESS);

// POWER: single-shot, MEDIUM repeatability, no clock stretching (datasheet Table 9).
// 6 ms max measurement vs 15 ms for the library's read(false) (high repeatability).
// Noise is 0.08 C / 0.15 %RH (3 sigma) -- well below the report-on-change thresholds.
static constexpr uint16_t SHT3X_MEAS_MEDIUM = 0x240B;
static constexpr uint32_t SHT3X_MEAS_WAIT_MS = 7;  // 6 ms max + 1 ms margin


//*********************************************************************
//    State kept across deep sleep (RTC memory; reset on power-up)
//*********************************************************************

RTC_DATA_ATTR float rtcLastTemp = NAN;  // last CONFIRMED reported values
RTC_DATA_ATTR float rtcLastHumid = NAN;
RTC_DATA_ATTR uint8_t rtcWakesSinceReport = HEARTBEAT_WAKES;        // force report after power-up
RTC_DATA_ATTR uint8_t rtcWakesSinceBattery = BATTERY_REPORT_WAKES;  // force battery after power-up
RTC_DATA_ATTR uint8_t rtcBattPct = 100;                             // last measured, used as attribute default
RTC_DATA_ATTR uint8_t rtcBattV100mV = 35;
RTC_DATA_ATTR uint8_t rtcFailCount = 0;  // consecutive wakes with no confirmed report


//*********************************************************************
//    Globals
//*********************************************************************

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);

// Per-attribute confirmation tracking (replaces the shared dataToSend counter,
// so a resend only re-sends what wasn't confirmed).
enum : uint8_t { PEND_TEMP = 0x01,
                 PEND_HUMID = 0x02,
                 PEND_BATT = 0x04 };
volatile uint8_t pending = 0;
volatile bool reportFailed = false;


//*********************************************************************
//    Picewise Lookup Table to calculate battery percent
//*********************************************************************

// Calibration table: voltage (mV) -> percent. MUST be sorted ascending by mV.
struct SocPoint {
  uint16_t mV;
  uint8_t pct;
};

//--------------------------
// Battery Percent Table
//--------------------------
const SocPoint SOC_TABLE[] = {
      { 2700, 0 },  { 3325, 5 },  { 3395, 10 }, { 3457, 15 }, { 3524, 20 }, 
      { 3565, 25 }, { 3625, 30 }, { 3695, 35 }, { 3744, 40 }, { 3771, 45 }, 
      { 3795, 50 }, { 3825, 55 }, { 3855, 60 }, { 3894, 65 }, { 3945, 70 }, 
      { 3965, 75 }, { 3976, 80 }, { 3986, 85 }, { 4003, 90 }, { 4025, 95 }, 
      { 4120, 100 }
};
const uint8_t SOC_TABLE_LEN = sizeof(SOC_TABLE) / sizeof(SOC_TABLE[0]);

/*
 * Return battery percentage (0-100) for a given cell voltage in VOLTS.
 * Uses linear interpolation between the two nearest table points.
 */
float batteryPercent(float volts) {
  uint16_t mV = (uint16_t)(volts * 1000.0f + 0.5f);  // volts -> millivolts

  // Clamp to the ends of the table.
  if (mV <= SOC_TABLE[0].mV) return 0.0f;
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
//    Helpers
//*********************************************************************

static inline void satInc(uint8_t &v) {
  if (v < 255) v++;
}

// Sleep time grows 2x per consecutive failure, capped at MAX_BACKOFF_SEC.
uint32_t sleepSeconds() {
  uint32_t s = TIME_TO_SLEEP_SEC;
  for (uint8_t i = 0; i < rtcFailCount && s < MAX_BACKOFF_SEC; i++) s *= 2;
  return (s > MAX_BACKOFF_SEC) ? MAX_BACKOFF_SEC : s;
}

// POWER: trigger a medium-repeatability measurement directly over I2C, then let
// the library read the 6 result bytes. readData(false) = verify both CRCs, so a
// corrupted read is rejected instead of being reported to Home Assistant.
bool readSHT31Medium() {
  Wire.beginTransmission(SHT31_ADDRESS);
  Wire.write((uint8_t)(SHT3X_MEAS_MEDIUM >> 8));
  Wire.write((uint8_t)(SHT3X_MEAS_MEDIUM & 0xFF));
  if (Wire.endTransmission() != 0) return false;
  delay(SHT3X_MEAS_WAIT_MS);
  return sht.readData(false);
}


//*********************************************************************
//    Zigbee report-confirmation callback (runs in the Zigbee task)
//*********************************************************************
void onGlobalResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status, uint8_t endpoint, uint16_t cluster) {
  Log.trace("Zigbee response cmd: %d, status: %s, ep: %u, cluster: 0x%04x" CR,
            command, esp_zb_zcl_status_to_name(status), endpoint, cluster);
  if ((command != ZB_CMD_REPORT_ATTRIBUTE) || (endpoint != TEMP_SENSOR_ENDPOINT_NUMBER)) return;

  if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
    uint8_t done = 0;
    switch (cluster) {
      case ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT: done = PEND_TEMP; break;
      case ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT: done = PEND_HUMID; break;
      case ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG: done = PEND_BATT; break;
      default: break;
    }
    pending = (uint8_t)(pending & ~done);
  } else {
    reportFailed = true;
  }
}


//*********************************************************************
//    Sleep (single exit point)
//*********************************************************************
void go_to_sleep(uint32_t seconds) {
#if LOG_LEVEL > LOG_LEVEL_SILENT
  Log.info("Sleeping %u s" CR, seconds);
  Serial.flush();
#endif
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * uS_TO_SEC_FACTOR);
  esp_deep_sleep_start();
  while (true) {}  // not reached
}


//*********************************************************************
//    Send the requested reports and wait for per-attribute confirmation.
//    Returns the bitmask of attributes that were confirmed.
//*********************************************************************
uint8_t reportAndWait(uint8_t which) {
  pending = which;

  for (uint8_t attempt = 0; attempt <= REPORT_RETRIES; attempt++) {
    reportFailed = false;
    uint8_t toSend = pending;  // only what is still unconfirmed
    if (toSend == 0) break;
    if (attempt > 0) Log.warning("Resending unconfirmed reports: 0x%x" CR, toSend);

    if (toSend & PEND_TEMP) zbTempSensor.reportTemperature();
    if (toSend & PEND_HUMID) zbTempSensor.reportHumidity();
    if (toSend & PEND_BATT) zbTempSensor.reportBatteryPercentage();

    // POWER: poll every 10 ms (was 50 ms) so we sleep right after the last confirmation.
    unsigned long start = millis();
    while (pending != 0 && !reportFailed && (millis() - start) < REPORT_TIMEOUT_MS) {
      delay(10);
    }
  }
  return which & ~pending;
}


//*********************************************************************
//    Setup (main)
//*********************************************************************
void setup() {
  // POWER: don't start the serial port at all when logging is silent.
#if LOG_LEVEL > LOG_LEVEL_SILENT
  Serial.begin(115200);
  delay(1000);  // give the serial port time to start before logging
#endif
  Log.begin(LOG_LEVEL, &Serial);
  Log.info("Booting..." CR);

  //-------------------------------------------------------------------
  // Maintenance halt / forced rejoin
  //-------------------------------------------------------------------
  pinMode(MONITOR_PIN, INPUT_PULLUP);
  bool forceRejoin = false;
  if (digitalRead(MONITOR_PIN) == LOW) {
    while (digitalRead(MONITOR_PIN) == LOW) {
      //Stay alive to update firmware
      //pinMode(LED_BUILTIN, OUTPUT);
      //digitalWrite(LED_BUILTIN, LOW);  // LED ON
      Log.info("CPU idle, ready to be flashed..." CR);
      delay(1000);
    }
    //digitalWrite(LED_BUILTIN, HIGH);  // LED OFF
    forceRejoin = true;
  }

  if (forceRejoin) {
    // Start fresh: force a full report on this boot.
    rtcLastTemp = NAN;
    rtcLastHumid = NAN;
    rtcWakesSinceReport = HEARTBEAT_WAKES;
    rtcWakesSinceBattery = BATTERY_REPORT_WAKES;
    rtcFailCount = 0;
  }

  //-------------------------------------------------------------------
  // POWER: read the sensor BEFORE starting Zigbee, so the measurement
  // doesn't happen with the receiver on (~75 mA), and we can decide
  // whether this wake needs the radio at all.
  //-------------------------------------------------------------------
  Wire.begin();
  Wire.setClock(400000);

  // POWER: the SHT31 stays powered through deep sleep and reloads its
  // calibration before every measurement, so only soft-reset it after a
  // power-up or reset -- not on every timer wake.
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    sht.begin();  // soft reset (library waits 1 ms)
    delay(1);     // datasheet allows up to 1.5 ms before commands are accepted
  }

  bool shtOk = readSHT31Medium();
  if (!shtOk) {
    Log.error(F("FAILED: Unable to read SHT31, resetting and trying again..." CR));
    sht.begin();  // recover a sensor in an unexpected state
    delay(20);
    shtOk = readSHT31Medium();
  }
  float temp = shtOk ? sht.getTemperature() : NAN;  // ZCL temperature is always Celsius
  float humid = shtOk ? sht.getHumidity() : NAN;

  satInc(rtcWakesSinceReport);
  satInc(rtcWakesSinceBattery);

  // A failed previous attempt or a forced rejoin also counts as "heartbeat due".
  bool heartbeatDue = (rtcWakesSinceReport >= HEARTBEAT_WAKES) || (rtcFailCount > 0) || forceRejoin;

  bool tempDue = shtOk && (heartbeatDue || isnan(rtcLastTemp) || fabsf(temp - rtcLastTemp) >= TEMP_DELTA_C);
  bool humidDue = shtOk && (heartbeatDue || isnan(rtcLastHumid) || fabsf(humid - rtcLastHumid) >= HUMID_DELTA_PCT);
  bool battDue = (rtcWakesSinceBattery >= BATTERY_REPORT_WAKES);
  if (heartbeatDue && !shtOk) battDue = true;  // still send something to keep the link alive

  // Battery rides along only when the radio is needed anyway.
  bool radioNeeded = tempDue || humidDue || (heartbeatDue && battDue);

  Log.info("T: %F C  H: %F  shtOk:%d  heartbeat:%d  radio:%d" CR,
           temp, humid, shtOk, heartbeatDue, radioNeeded);

  if (!radioNeeded) {
    // POWER: nothing worth sending -> back to sleep without starting Zigbee.
    go_to_sleep(TIME_TO_SLEEP_SEC);
  }

  //-------------------------------------------------------------------
  // POWER: battery ADC only when the battery report is due.
  //-------------------------------------------------------------------
  float battVoltage = 0.0f;
  float battPercent = 0.0f;
  if (battDue) {
    uint32_t batt_level = 0;
    for (int i = 0; i < 16; i++) {
      batt_level += analogReadMilliVolts(A1);  // GPIO1/D1
    }
    // Battery is monitored through a 1:2 voltage divider.
    battVoltage = ((batt_level / 16) / 1000.0f) * 2.0f;
    battPercent = batteryPercent(battVoltage);
    rtcBattPct = (uint8_t)(battPercent + 0.5f);
    rtcBattV100mV = (uint8_t)(battVoltage * 10.0f + 0.5f);
    Log.info("Batt: %F%%  BattV: %F" CR, battPercent, battVoltage);
  }

  //-------------------------------------------------------------------
  // Zigbee setup
  //-------------------------------------------------------------------
  //if (forceRejoin) {
  //  // Erase Zigbee NVS so begin() does a full network scan + join.
  //  Zigbee.factoryReset(false);
  //}

  zbTempSensor.setManufacturerAndModel("odelay.io", "XIAO ESP32-C6 + SHT31");
  zbTempSensor.setMinMaxValue(-40, 80);
  zbTempSensor.setTolerance(0.5);

  // Use the last measured battery values as the attribute defaults.
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, rtcBattPct, rtcBattV100mV);
  zbTempSensor.addHumiditySensor(0, 100, 1, 0.0);

  Zigbee.onGlobalDefaultResponse(onGlobalResponse);
  Zigbee.addEndpoint(&zbTempSensor);

  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;

  // POWER: shorter rejoin scan (see ZIGBEE_SCAN_DURATION above).
  // NOTE: I tried ZIGBEE_SCAN_DURATION = 1, and it increased timeouts
  Zigbee.setScanDuration(ZIGBEE_SCAN_DURATION);

  // EXPERIMENT (measure with the PPK2, then re-pair the device in Z2M/ZHA):
  // declare the device as a sleepy end device (receiver off when idle).
  // The library default is true. If report confirmations start timing out,
  // remove this line again.
  // Zigbee.setRxOnWhenIdle(false);

  Zigbee.setTimeout(ZIGBEE_BEGIN_TIMEOUT_MS);

  // NOTE: Zigbee.setRebootOpenNetwork() was removed. In the Arduino Zigbee
  // library it only takes effect on a coordinator; on an end device it did
  // nothing. Enable "Permit join" in ZHA/Zigbee2MQTT when pairing instead.

  if (!Zigbee.begin(&zigbeeConfig, false)) {
    // POWER: previously ESP.restart() -- with the coordinator offline that
    // loops forever with the radio on (~10 s per try, never sleeping).
    Log.warning("Zigbee failed to start! Backing off..." CR);
    satInc(rtcFailCount);
    go_to_sleep(sleepSeconds());
  }

  Log.info("Connecting to Zigbee network..." CR);
  unsigned long joinStart = millis();
  while (!Zigbee.connected() && (millis() - joinStart) < ZIGBEE_JOIN_TIMEOUT_MS) {
    delay(10);
  }
  if (!Zigbee.connected()) {
    Log.warning("Not connected to Zigbee network! Backing off..." CR);
    satInc(rtcFailCount);
    go_to_sleep(sleepSeconds());
  }
  Log.info("Connected to Zigbee network" CR);

  //-------------------------------------------------------------------
  // Report only what is due, then update the RTC state
  //-------------------------------------------------------------------
  uint8_t which = 0;
  if (tempDue) {
    zbTempSensor.setTemperature(temp);
    which |= PEND_TEMP;
  }
  if (humidDue) {
    zbTempSensor.setHumidity(humid);
    which |= PEND_HUMID;
  }
  if (battDue) {
    zbTempSensor.setBatteryPercentage(rtcBattPct);
    zbTempSensor.setBatteryVoltage(rtcBattV100mV);  // units of 100 mV (not reportable)
    which |= PEND_BATT;
  }

  uint8_t confirmed = reportAndWait(which);
  Log.info("Reports requested 0x%x, confirmed 0x%x" CR, which, confirmed);

  if (confirmed & PEND_TEMP) rtcLastTemp = temp;
  if (confirmed & PEND_HUMID) rtcLastHumid = humid;
  if (confirmed & PEND_BATT) rtcWakesSinceBattery = 0;

  if (confirmed != 0) {
    rtcWakesSinceReport = 0;
    rtcFailCount = 0;
    go_to_sleep(TIME_TO_SLEEP_SEC);
  } else {
    satInc(rtcFailCount);
    go_to_sleep(sleepSeconds());
  }
}

void loop() {
  // never reached
}
