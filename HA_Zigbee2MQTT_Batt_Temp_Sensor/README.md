Converted the sketch from WiFi/MQTT to native Zigbee, using the official `ZigbeeTempSensor` endpoint class (Arduino-ESP32 core's Zigbee library, verified against Espressif's `Zigbee_Temp_Hum_Sensor_Sleepy` reference example, which explicitly targets ESP32-C6/H2).

What changed in [HA_Zigbee_MQTT_Batt_Temp_Sensor.ino](vscode-webview://03jjvne9ajfpemnqpnf12amb1bnbbh6etmal4dhrj8fee1p3fdru/HA_Zigbee_MQTT_Batt_Temp_Sensor/HA_Zigbee_MQTT_Batt_Temp_Sensor/HA_Zigbee_MQTT_Batt_Temp_Sensor.ino):

- **Removed**: `ArduinoMqttClient`, `WiFi`/`WiFiClient`, `arduino_secrets.h`, the MAC-derived device ID, MQTT discovery-config JSON builder, and all RTC-cached WiFi reconnect state. No broker, no credentials, no `configuration.yaml` edits.
- **Added**: a Zigbee **sleepy end device** with one endpoint exposing the standard ZCL Temperature Measurement, Relative Humidity Measurement, and Power Configuration (battery) clusters. Home Assistant creates the Temperature/Humidity/Battery entities automatically via ZHA or Zigbee2MQTT.
- **Kept unchanged**: the SHT31 read, the battery-voltage ADC/divider logic, and your full LiPo discharge-curve `SOC_TABLE`/`batteryPercent()` — none of that is protocol-specific.
- **MONITOR_PIN (D3)** now factory-resets the Zigbee stack on release (forces a full rejoin) instead of forcing an MQTT discovery re-publish — same "hold to intervene at boot" pattern, translated to the new protocol.
- Temperature is now always reported in **Celsius** — the ZCL cluster has no Fahrenheit mode; HA converts for display based on the user's profile, so the old `USE_FAHRENHEIT` toggle is gone.

Before building, in Arduino IDE: **Tools → Board**: Seeed XIAO ESP32-C6 (this is the one XIAO board with the 802.15.4 radio Zigbee needs — the ESP32-C3/S3 XIAO boards can't do this). **Tools → Zigbee mode**: "Zigbee ED (end device)". **Tools → Partition Scheme**: "Zigbee 4MB with spiffs".

One thing to flag: `arduino_secrets.h` in that folder is no longer referenced by the sketch, but it still has real-looking WiFi/MQTT/Blynk credentials in it (apparently copied from another project). I left the file alone rather than touch secrets — let me know if you want it deleted or cleared out.

I couldn't compile-check this locally (no `arduino-cli`/PlatformIO installed here), so please do a build/verify in the Arduino IDE before flashing.