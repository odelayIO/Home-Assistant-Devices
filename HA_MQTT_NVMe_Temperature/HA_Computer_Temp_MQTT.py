#!/usr/bin/python3

#############################################################################################
#############################################################################################
#
#   The MIT License (MIT)
#   
#   Copyright (c) 2026 http://odelay.io 
#   
#   Permission is hereby granted, free of charge, to any person obtaining a copy
#   of this software and associated documentation files (the "Software"), to deal
#   in the Software without restriction, including without limitation the rights
#   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#   copies of the Software, and to permit persons to whom the Software is
#   furnished to do so, subject to the following conditions:
#   
#   The above copyright notice and this permission notice shall be included in all
#   copies or substantial portions of the Software.
#   
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#   SOFTWARE.
#   
#   Contact : <everett@odelay.io>
#  
#   Description : Simple Python script to send NVMe temperature to Home Assistant
#           
#
#   Version History:
#   
#       Date        Description
#     -----------   -----------------------------------------------------------------------
#      2026-01-19    Original Creation
#
###########################################################################################

import os
import time
import logging
from logging.handlers import RotatingFileHandler
import paho.mqtt.client as mqtt



HWMON_PATH = "/sys/class/hwmon/hwmon1"
MQTT_BROKER = "nuc-sdr"
MQTT_PORT = 1883
LOG_FILE = os.path.join(os.path.dirname(__file__), "HA_Computer_Temp_MQTT.log")

# Map the sensor label text to the desired MQTT topic
LABEL_TO_TOPIC = {
    "Composite": "puch_rack/nvme/composite",
    "Sensor 1": "puch_rack/nvme/sensor_1",
    "Sensor 2": "puch_rack/nvme/sensor_2",
}


def read_temp(n):
    with open(f"{HWMON_PATH}/temp{n}_input") as f:
        return int(f.read().strip()) / 1000


def read_label(n):
    label_path = f"{HWMON_PATH}/temp{n}_label"
    if os.path.exists(label_path):
        with open(label_path) as f:
            return f.read().strip()
    return None


logger = logging.getLogger()
logger.setLevel(logging.INFO)
formatter = logging.Formatter("%(asctime)s %(levelname)s: %(message)s")
handler = RotatingFileHandler(LOG_FILE, maxBytes=1_048_576, backupCount=3)
handler.setFormatter(formatter)
logger.addHandler(handler)
#logger.info("NVMe Temperatures:")


def main():
    # collect discovered temps for the labels we care about
    found = {}

    for i in range(1, 10):
        input_path = f"{HWMON_PATH}/temp{i}_input"
        if not os.path.exists(input_path):
            continue

        label = read_label(i)
        if label in LABEL_TO_TOPIC:
            try:
                temp = read_temp(i)
            except Exception as e:
                logging.warning("Failed to read temp%d: %s", i, e)
                continue
            found[label] = temp
            #logging.info("%s: %.1f °C", label, temp)


    if not found:
        logging.info("No matching NVMe temperature labels found. Nothing will be published.")
        return

    # Setup MQTT client with credentials (use callback API v2 to avoid deprecation)
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
    except Exception as e:
        logging.error("Failed to connect to MQTT broker %s:%d - %s", MQTT_BROKER, MQTT_PORT, e)
        return

    # Start network loop so publishes are processed
    client.loop_start()

    for label, temp in found.items():
        topic = LABEL_TO_TOPIC.get(label)
        if not topic:
            continue
        payload = f"{temp:.1f}"
        try:
            client.publish(topic, payload, retain=True)
            logging.info("Published %s°C to %s", payload, topic)
        except Exception as e:
            logging.error("Failed to publish %s: %s", topic, e)
        # small pause to give the client time to send
        time.sleep(0.1)

    # give a short moment for remaining messages to be sent
    time.sleep(0.5)
    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()


