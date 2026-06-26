# HA Power Controller for the puch Rack

Information on the puch framework and the rack is found on: https://puch.readthedocs.io



## MQTT Topics

The file looks clean and correct. Here's a summary of the new MQTT topic layout for your Home Assistant configuration:

**Status topics** (XIAO publishes `ON`/`OFF` or speed):

| Device    | Topic                                                   |
| --------- | ------------------------------------------------------- |
| KRIA      | `puch_rack/kria/status`                                 |
| PYNQZ1    | `puch_rack/pynqz1/status`                               |
| PLUTO-1   | `puch_rack/pluto1/status`                               |
| PLUTO-2   | `puch_rack/pluto2/status`                               |
| Fan       | `puch_rack/fan/status` → `OFF` / `LOW` / `MED` / `HIGH` |
| WiFi RSSI | `puch_rack/wifi/rssi` → numeric dBm value               |

**Command topics** (HA publishes to these):

| Device  | Topic                      | Valid payloads              |
| ------- | -------------------------- | --------------------------- |
| KRIA    | `puch_rack/kria/command`   | `ON`, `OFF`                 |
| PYNQZ1  | `puch_rack/pynqz1/command` | `ON`, `OFF`                 |
| PLUTO-1 | `puch_rack/pluto1/command` | `ON`, `OFF`                 |
| PLUTO-2 | `puch_rack/pluto2/command` | `ON`, `OFF`                 |
| Fan     | `puch_rack/fan/command`    | `OFF`, `LOW`, `MED`, `HIGH` |

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
    state_topic: "puch_rack/wifi/rssi"
    unit_of_measurement: "dBm"
    device_class: signal_strength
    state_class: measurement
    qos: 1
    unique_id: "puch_rack_rssi_001"


select:
  - name: "Rack Fan Mode"
    state_topic: "puch_rack/fan/status"
    command_topic: "puch_rack/fan/command"
    options:
      - "OFF"
      - "LOW"
      - "MED"
      - "HIGH"
    qos: 1
    retain: false
    optimistic: false
    unique_id: "rack_fan_mode_001"

switch:
  - name: "KRIA Board"
    state_topic: "puch_rack/kria/status"
    command_topic: "puch_rack/kria/command"
    payload_on: "ON"
    payload_off: "OFF"
    qos: 1
    retain: false
    unique_id: "kria_board_001"
  - name: "PYNQ-Z1 Board"
    state_topic: "puch_rack/pynqz1/status"
    command_topic: "puch_rack/pynqz1/command"
    payload_on: "ON"
    payload_off: "OFF"
    qos: 1
    retain: false
    unique_id: "pynqz1_board_001"
  - name: "Pluto SDR-1"
    state_topic: "puch_rack/pluto1/status"
    command_topic: "puch_rack/pluto1/command"
    payload_on: "ON"
    payload_off: "OFF"
    qos: 1
    retain: false
    unique_id: "pluto_sdr_1_001"
  - name: "Pluto SDR-2"
    state_topic: "puch_rack/pluto2/status"
    command_topic: "puch_rack/pluto2/command"
    payload_on: "ON"
    payload_off: "OFF"
    qos: 1
    retain: false
    unique_id: "pluto_sdr_2_001"

```



### Home Assistant Lovelace UI

Below is the Lovelace UI YAML source code.

```yaml
type: entities
entities:
  - entity: switch.pynq_kria_plug
    name: Main Power
  - entity: sensor.pynq_kria_plug_power
    name: Main Power Load
  - entity: sensor.puch_dev_wks_nvme_temperature
    name: NVMe Composite
  - entity: sensor.puch_dev_wks_nvme_temperature_1
    name: NVMe Die
  - entity: select.rack_fan_mode_2
    name: NVMe Fan Speed
  - entity: switch.kria_board
  - entity: switch.pynq_z1_board
  - entity: switch.pluto_sdr_1
  - entity: switch.pluto_sdr_2
title: puch Rack

```

