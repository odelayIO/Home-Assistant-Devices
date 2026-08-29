# Arduino-ESP32 Zigbee Router + On/Off Light Example

This example shows how to configure a Zigbee Router device that acts as a Home Automation (HA) network range extender **and** exposes a controllable on/off light endpoint on the same device.

Range extension is simply a property of running in router mode (`Zigbee.begin(ZIGBEE_ROUTER)` / router config) - any router relays traffic for other devices on the mesh. The dedicated `ZigbeeRangeExtender` endpoint used by the `Zigbee_Range_Extender` example is meant to be used on its own and is unreliable when combined with another functional endpoint (the light wouldn't respond to commands, or Zigbee2MQTT wouldn't expose it correctly). So this sketch drops that endpoint and just adds a single `ZigbeeLight` endpoint (`ZIGBEE_LIGHT_ENDPOINT`) to a router-mode device - you get the range extension for free, plus a light that can be toggled from a coordinator/hub (e.g. Home Assistant via ZigbeeHomeAssistant/Zigbee2MQTT) or from the onboard button.

> **If you previously flashed this device as a plain `Zigbee_Range_Extender`,** Zigbee2MQTT/your coordinator may have cached the old (lightless) endpoint list from its last interview. Remove and re-pair the device, or trigger "Reinterview" on it, so the light endpoint is picked up.

To see if the communication with your Zigbee network works, use the Serial monitor and watch for output there.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* A USB cable for power supply and programming
* Board (ESP32-H2 or ESP32-C6) as Zigbee router device and upload the Zigbee_Range_Extender_Light example
* Zigbee network / coordinator (Other board with switch examples or Zigbee2mqtt or ZigbeeHomeAssistant like application)

### Configure the Project

Set the LED GPIO by changing the `LED_BUILTIN` definition if needed. The LED reflects the light on/off state and also blinks during Zigbee identify.

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the Coordinator/Router device Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs` (select correct size)
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.

## Usage

* Press the button briefly to toggle the light on/off locally.
* Hold the button for more than 3 seconds to factory reset the Zigbee stack and reboot.
* The light can also be toggled remotely from the Zigbee coordinator/hub once joined to the network.

## Troubleshooting

If the Router device flashed with this example is not connecting to the coordinator, erase the flash of the Router device before flashing the example to the board. It is recommended to do this if you re-flash the coordinator.
You can do the following:

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.
* Add to the sketch `Zigbee.factoryReset();` to reset the device and Zigbee stack.

By default, the coordinator network is closed after rebooting or flashing new firmware.
To open the network you have 2 options:

* Open network after reboot by setting `Zigbee.setRebootOpenNetwork(time);` before calling `Zigbee.begin();`.
* In application you can anytime call `Zigbee.openNetwork(time);` to open the network for devices to join.

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

* **LED not blinking:** Check the wiring connection and the IO selection.
* **Programming Fail:** If the programming/flash procedure fails, try reducing the serial connection speed.
* **COM port not detected:** Check the USB cable and the USB to Serial driver installation.

If the error persists, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32-C6 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
* ESP32-H2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
