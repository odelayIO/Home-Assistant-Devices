# Home-Assistant-Devices
Contains HA configuration files and designs for custom devices

Clone `Home-Assistant-Devices`:

```bash
git clone --recursive git@github.com:odelayIO/Home-Assistant-Devices.git
```

## WiFi and MQTT Credentials

Each Arduino sketch folder contains an `arduino_secrets.example` template.  Rename (or copy) it to `arduino_secrets.h` in the same folder and fill in your WiFi and MQTT credentials before building the firmware.  `arduino_secrets.h` is listed in `.gitignore`, so the file will be ignored when committing changes to the repository and your credentials stay local.

**Under Construction**

To Do:

- Create list of Arduino Library used to build the HA devices
- Update all device source code with documentation
- Provide HA configuration and template instructions
- Provide HA Docker configuration
- Update HA devices with Arduino Log library
  - All Arduino devices use the Serial port, want to add the ability to remove Serial port messages for deployed devices  

