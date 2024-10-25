**Documentation is In-Progress**

# XIAO MQTT Battery Temperature and Humidity Sensor

Using the XIAO for a battery operated temperature and humidity sensor.  The goal is to operate this sensor for 6 months using a 1400 mAh battery.  A 750 mAh battery is being used for the initial design release with a 3 month operation goal.  I initially was operating the sensor without using the deep sleep mode to validate the initial battery life calculations.  The latest update committed to the repository has deep sleep implemented.  

![assembled](./doc/assembled.jpg)

![open-1](./doc/open-1.jpg)

Source: https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/

[ESP32-C3 Datasheet](./doc/esp32-c3_datasheet.pdf)





## XIAO ESP32C3 Wiring

### Temperature and Humidity Sensor (SHT-31-D) Device



| Pin          | Description |
| ------------ | ----------- |
| SDA/GPIO6/D4 | SDA         |
| SCL/GPIO7/D5 | SCL         |
| 3v3          | Power       |
| GND          | Ground      |

### Battery Life Reading

Used 300Kohm resistors in series with the ADC A0 attached to the voltage divider circuit.  

Source: https://forum.seeedstudio.com/t/battery-voltage-monitor-and-ad-conversion-for-xiao-esp32c/267535



## Battery Testing

The XIAO has on-board battery charging circuit with protection circuit.  It seems to work like a normal battery pack, a red LED is on while charging and will turn off when the battery is charged.  Home Assistant reports a full charge is 81.5%, and the device stops operating at ~53%.  An update will be made in the next firmware release to scale this correctly.

Power Consumption with battery removed, and running with wall power using a USB power meter.  

Idle Mode: 0.153W (0.0306A).  

Transmitting a MQTT message: 1.24W (0.248A). 

The USB power meter used measures 0W while in deep sleep.



### 750mAh Battery Test without Deep Sleep

| Parameter Description                  | Value     |
| -------------------------------------- | --------- |
| Battery Capacity                       | 750 mAh   |
| Device Consumption During MQTT Message | 278 mA    |
| Device Consumption Idle                | 28 mA     |
| Transmit Duration                      | 80 ms     |
| Device Idle Duration                   | 920 ms    |
| Estimated Battery Life                 | ~20 hours |



### 1400mAh Battery Test without Deep Sleep

| Parameter Description                  | Value     |
| -------------------------------------- | --------- |
| Battery Capacity                       | 1400 mAh  |
| Device Consumption During MQTT Message | 278 mA    |
| Device Consumption Idle                | 28 mA     |
| Transmit Duration                      | 80 ms     |
| Device Idle Duration                   | 920 ms    |
| Estimated Battery Life                 | ~38 hours |



#### Measuring Message Duration

Measured duration to send the MQTT message including the following lines of code:

```c
    Serial.print("Message Time: ");
    Serial.print(message_time);
    Serial.println(" ms");
```

Just uncomment the lines of code at the end of the `if` statement, and it should be around `80 ms`.

```bash
Message Time: 82 ms
Message Time: 78 ms
Message Time: 73 ms
Message Time: 77 ms
Message Time: 77 ms
Message Time: 82 ms
Message Time: 83 ms
Message Time: 81 ms
Message Time: 72 ms
Message Time: 78 ms
Message Time: 77 ms
Message Time: 79 ms
Message Time: 76 ms
Message Time: 77 ms
Message Time: 75 ms
Message Time: 74 ms
Message Time: 75 ms
```



### Manually Downloading Updates

```bash
C:\Users\eston\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources>arduino-cli.exe core update-index

Remove:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```



Note:  Closing the `Serial Monitor` will resolve the update hang issue.
