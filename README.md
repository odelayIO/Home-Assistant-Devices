# Home-Assistant-Devices
Contains HA configuration files and designs for custom devices

Clone `Home-Assistant-Devices`:

```bash
git clone --recursive git@github.com:odelayIO/Home-Assistant-Devices.git
```

## WiFi and MQTT Credentials

Each Arduino sketch folder contains an `arduino_secrets.example` template.  Rename (or copy) it to `arduino_secrets.h` in the same folder and fill in your WiFi and MQTT credentials before building the firmware.  `arduino_secrets.h` is listed in `.gitignore`, so the file will be ignored when committing changes to the repository and your credentials stay local.

## HA Devices Description

| Device | Board | Protocol | Description |
| --- | --- | --- | --- |
| [HA_MQTT_Auto_Discover_Temp_Sensor](./HA_MQTT_Auto_Discover_Temp_Sensor) | Generic XIAO ESP32 | WiFi/MQTT (Auto-Discovery) | Battery-powered SHT31 temperature/humidity sensor with a piecewise LiPo state-of-charge table and deep-sleep power optimization; publishes only on temperature delta or heartbeat interval. |
| [HA_MQTT_Auto_Discover_Temp_Sensor_Guage](./HA_MQTT_Auto_Discover_Temp_Sensor_Guage) | Generic XIAO ESP32 | WiFi/MQTT (Auto-Discovery) | Same auto-discovered SHT31 temperature/humidity sensor, upgraded to use a MAX17048 fuel gauge IC for battery percentage instead of a resistor-divider ADC reading. |
| [HA_MQTT_Batt_Temperature_ane_Humidity_Sensor](./HA_MQTT_Batt_Temperature_ane_Humidity_Sensor) | Seeed XIAO ESP32-C3 | WiFi/MQTT | Battery-operated temperature/humidity sensor (small wine fridge) targeting 3-6 months on a single 750-1400 mAh battery using deep sleep. |
| [HA_MQTT_Blynk_Planter_Sensor](./HA_MQTT_Blynk_Planter_Sensor) | Arduino Nano ESP32 | WiFi/Blynk | Wireless soil moisture sensor for a planter, reporting to a Blynk iOS/Android app and web dashboard. |
| [HA_MQTT_Garage_Sensor](./HA_MQTT_Garage_Sensor) | NodeMCU-32S | WiFi/MQTT | Garage door position sensor using an HC-SR04 ultrasonic distance sensor. |
| [HA_MQTT_NVMe_Temperature](./HA_MQTT_NVMe_Temperature) | Linux host (Python script) | WiFi/MQTT | Systemd-timer-driven Python script that publishes a computer's NVMe (or other) temperature to Home Assistant. |
| [HA_MQTT_puch_rack_controller](./HA_MQTT_puch_rack_controller) | XIAO ESP32-C3 | WiFi/MQTT | Web/phone-controlled power controller for a "puch" server rack, exposing four switches and a fan as independent MQTT entities. |
| [HA_MQTT_Temperature_and_Humidity_Sensor](./HA_MQTT_Temperature_and_Humidity_Sensor) | Arduino Nano ESP32 | WiFi/MQTT | Mains-powered SHT-31-D temperature/humidity sensor (main wine fridge) publishing updates every 2 seconds. |
| [HA_X10_MQTT_Bridge](./HA_X10_MQTT_Bridge) | Arduino Nano | WiFi/MQTT | Bridges legacy X10 home-automation devices to Home Assistant over MQTT. |
| [HA_Zigbee2MQTT_Batt_Temp_Sensor](./HA_Zigbee2MQTT_Batt_Temp_Sensor) | Seeed XIAO ESP32-C6 | Native Zigbee (sleepy end device) | Battery-powered SHT31 temperature/humidity/battery sensor exposed via standard ZCL clusters (Zigbee2MQTT/ZHA auto-creates entities); no WiFi/MQTT credentials needed. |
| [HA_Zigbee2MQTT_OnOff_LED](./HA_Zigbee2MQTT_OnOff_LED) | ESP32-C6 / ESP32-H2 | Native Zigbee (end device) | Zigbee on/off light end-device example (LED) that joins a Zigbee2MQTT/ZHA network as an HA on/off light. |
| [HA_Zigbee2MQTT_Router](./HA_Zigbee2MQTT_Router) | ESP32-C6 / ESP32-H2 | Native Zigbee (router) | Zigbee router device that extends network range while also exposing an on/off light endpoint on the same device. |
