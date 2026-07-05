# HA X10 MQTT Bridge

**Documentation is In-Progress**

Bridges X10 devices to Home Assistant over MQTT using an Arduino Nano (`Nano_X10_MQTT_Bridge/` sketch).

## WiFi and MQTT Credentials

The firmware reads the WiFi and MQTT broker credentials from `arduino_secrets.h`, which is not stored in the repository.  Before building, rename (or copy) `arduino_secrets.example` to `arduino_secrets.h` in the `Nano_X10_MQTT_Bridge/` sketch folder and fill in your credentials:

```c
#define SECRET_SSID "your-wifi-ssid"
#define SECRET_PASS "your-wifi-password"

#define SECRET_MQTT_USER "your-mqtt-username"
#define SECRET_MQTT_PASS "your-mqtt-password"
```

`arduino_secrets.h` is listed in the repository's `.gitignore`, so the file will be ignored when committing changes and your credentials stay local.
