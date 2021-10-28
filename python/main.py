import queue
import os

import eea_runtime_wasmer as eea
#import eea_runtime_pywasm3 as eea
import eea_mqtt
import eea_accelerometer

# Device authentication details.
DEVICE_ID = os.environ.get("DEVICE_ID")
ACCESS_KEY = os.environ.get("ACCESS_KEY")
ACCESS_SECRET = os.environ.get("ACCESS_SECRET")
  
#
# Initialize the workers and create the queues that are
# used to communicate between threads.
#
if __name__ == "__main__":

  # If the device is not connected and your workflows
  # are sending messages, this queue can grow indefinitely.
  # In practice, probably a good idea add a max size.
  mqtt_queue = queue.Queue()
  eea_queue = queue.Queue()

  # Initialize the EEA.
  eea.init(DEVICE_ID, eea_queue, mqtt_queue)

  # Initialize the MQTT client.
  eea_mqtt.init(DEVICE_ID, ACCESS_KEY, ACCESS_SECRET, eea_queue, mqtt_queue)

  # Initialize the accelerometer sensor.
  #eea_accelerometer.init()