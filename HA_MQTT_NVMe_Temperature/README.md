# HA Computer Temperature Monitor

Just a Python script that will send the computer temperature for the NVMe to Home Assistant (HA).  This could be updated to monitor any temperature on the computer system.

## Quick Start

Copy the `HA_Computer_Temp_MQTT.py` to a local directory.  The Python script will be executed by a system daemon.

Copy `HA_Computer.timer` and `HA_Computer.service` to `/etc/systemd/system/*`

```bash
cp HA_Computer_Temp.py /path/to/desired/location/.
sudo cp HA_Computer.timer /etc/systemd/system/.
sudo cp HA_Computer.service /etc/systemd/system/.
```

Now you want to register the `timer` in the system daemon:

```bash
sudo systemctl daemon-reload
sudo systemctl enable HA_Computer.timer
sudo systemctl start HA_Computer.timer
```

Check the service is running:

```bash
sudo systemctl status HA_Temperature.service
sudo systemctl status HA_Temperature.timer
```

To check the log file:

```bash
sudo journalctl -u HA_Temperature.service
```





## Home Assistant Integration

![HA-webpage-controller](./doc/HA-webpage-controller.png)

### HA MQTT Configuration

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



## Journal Recommend Settings

This prevents runaway logs from filling your disk (especially useful for services that run every 10 seconds, like your MQTT script).

### ✅ Limit the Maximum Log Size

Edit the config:

```
sudo nano /etc/systemd/journald.conf
```

Uncomment or add one (or more) of these:

#### **Limit total disk usage**

```
SystemMaxUse=500M
```

#### **Limit per-file size**

```
SystemMaxFileSize=50M
```

#### **Keep only recent logs**

```
MaxRetentionSec=7day
```

#### **Limit number of files**

```
SystemMaxFiles=10
```

------

### Apply the Changes

```
sudo systemctl restart systemd-journald
```

------

### 🔍 Check Current Disk Usage

```
journalctl --disk-usage
```

------

### 🧹 Manually Clean Old Logs

Free space immediately:

```
sudo journalctl --vacuum-size=200M
```

Or by age:

```
sudo journalctl --vacuum-time=3d
```

