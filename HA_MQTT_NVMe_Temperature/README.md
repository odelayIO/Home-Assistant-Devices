1. # HA Computer Temperature Monitor

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

To check the log file:

```bash
sudo journalctl -u HA_Computer.service
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

------

### Recommended Settings for a Home System

```
SystemMaxUse=200M
SystemMaxFileSize=20M
MaxRetentionSec=5day
```

