# HA Power Controller for the puch Rack

Information on the puch framework and the rack is found on: https://puch.readthedocs.io



## MQTT Topics

The four switches map directly to HA's `mqtt` switch integration, and the fan fits the `mqtt` fan (or `select`) entity. Each device now has completely isolated topics so HA can track their states independently.

This is the webpage/phone puch power controller for the puch rack.  This is implemented by sending MQTT message to a XIAO ESP32 C3 development board.



![HA-webpage-controller](./doc/HA-webpage-controller.png)



The HA-NVMe monitor is integrated in the HA-UI, see the Python device: https://github.com/odelayIO/Home-Assistant-Devices/tree/main/HA_MQTT_NVMe_Temperature





## Building Arduino Firmware

Using the ArduinoMQTTClient library for the MQTT message: https://github.com/arduino-libraries/ArduinoMqttClient



## Home Assistant Integration

### HA MQTT Configuration

The Home Assistant configuration is controlled by YAML file to define the custom devices.  Copy and paste the following YAML source code into the Home Assistant configuration files.

```yaml
sensor:
    - name: "puch dev-wks NVMe Temperature-Composite"
    state_topic: "puch_rack/nvme/composite"
    qos: 1
    unit_of_measurement: "°C"
    icon: mdi:thermometer
    suggested_display_precision: 2
    unique_id: "puch_rack_nvme_temp_001"
  - name: "puch dev-wks NVMe Temperature-1"
    state_topic: "puch_rack/nvme/sensor_1"
    qos: 1
    unit_of_measurement: "°C"
    icon: mdi:thermometer
    suggested_display_precision: 2
    unique_id: "puch_rack_nvme_temp_002"
  - name: "puch dev-wks NVMe Temperature-2"
    state_topic: "puch_rack/nvme/sensor_2"
    qos: 1
    unit_of_measurement: "°C"
    icon: mdi:thermometer
    suggested_display_precision: 2
    unique_id: "puch_rack_nvme_temp_003"
  - name: "puch Rack WiFi RSSI"
    state_topic: "puch_rack/WiFiRSSI"
    unit_of_measurement: "dBm"
    device_class: signal_strength
    state_class: measurement
    qos: 1
    unique_id: "puch_rack_rssi_001"

select:
  - name: "Rack Fan Mode"
    state_topic: "puch_rack/status"
    command_topic: "puch_rack/control"
    options:
      - "FAN-OFF"
      - "FAN-LOW"
      - "FAN-MED"
      - "FAN-HIGH"
    qos: 1
    retain: false
    optimistic: false
    unique_id: "rack_fan_mode_001"

switch:
  - name: "KRIA Board"
    state_topic: "puch_rack/status"
    command_topic: "puch_rack/control"
    payload_on: "KRIA-ON"
    payload_off: "KRIA-OFF"
    qos: 1
    retain: false
    unique_id: "kria_board_001"
  - name: "PYNQ-Z1 Board"
    state_topic: "puch_rack/status"
    command_topic: "puch_rack/control"
    payload_on: "PYNQZ1-ON"
    payload_off: "PYNQZ1-OFF"
    qos: 1
    retain: false
    unique_id: "pynqz1_board_001"
  - name: "Pluto SDR-1"
    state_topic: "puch_rack/status"
    command_topic: "puch_rack/control"
    payload_on: "PLUTO-1-ON"
    payload_off: "PLUTO-1-OFF"
    qos: 1
    retain: false
    unique_id: "pluto_sdr_1_001"
  - name: "Pluto SDR-2"
    state_topic: "puch_rack/status"
    command_topic: "puch_rack/control"
    payload_on: "PLUTO-2-ON"
    payload_off: "PLUTO-2-OFF"
    qos: 1
    retain: false
    unique_id: "pluto_sdr_2_001"


```



### Home Assistant Lovelace UI

Below is the Lovelace UI YAML source code.

```yaml
type: entities
entities:
  - entity: sensor.puch_rack_wifi_rssi
  - entity: switch.pynq_kria_plug
  - entity: sensor.pynq_kria_plug_power
  - entity: sensor.puch_dev_wks_nvme_temperature_1
  - entity: sensor.puch_dev_wks_nvme_temperature_2
  - entity: select.rack_fan_mode
  - entity: switch.kria_board
  - entity: switch.pynq_z1_board
  - entity: switch.pluto_sdr_1
  - entity: switch.pluto_sdr_2
title: puch Rack
show_header_toggle: false

```

