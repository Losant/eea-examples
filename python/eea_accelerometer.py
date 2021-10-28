#
# This module demonstrates how to read a sensor from another worker
# thread. The sensor, in this example, is writing accelerometer
# values to a serial port that is being read and made available
# to the EEA via a registered function.
#

import json
import serial
import threading
import time

# The accelerometer readings (comma-delimited string) constantly
# being updated by the serial thread. Each reading is 1024 items.
accel_readings = None

# Lock that gives multiple threads access to accel_readings.
accel_lock = threading.Lock()

sensor = serial.Serial()
sensor.port="/dev/serial/by-id/usb-Arduino_LLC_Arduino_MKR_WiFi_1010_2067E12D50573554322E3120FF15091B-if00"
sensor.baudrate=115200
sensor.timeout=5000

# The accelerometer worker thread.
def thread():

  global accel_readings

  while(True):
    try:
      if sensor.is_open:

        # One line is 1024, comma-delimited, values.
        # Add brackets on either side to make it a valid JSON array.
        readings = "[" + sensor.readline().strip().decode('utf-8') + "]"

        # Attempt to parse the result. This will raise an
        # exception if it fails to be caught by the except block.
        json.loads(readings)

        acquire_lock()
        accel_readings = readings
        release_lock()

      else:
        sensor.open()
    except Exception as e:

      # If this failed, clear out the readings and
      # sleep the thread a little to prevent a tight
      # loop if the interface is permanently down.
      accel_lock.acquire()
      accel_readings = None
      accel_lock.release()
      
      time.sleep(1)
      continue

def init():
  # Start the thread.
  threading.Thread(target=thread, args=()).start()

# Allows other threads to acquire the lock to
# safely access the accelerometer readings.
def acquire_lock():
  accel_lock.acquire()

# Allows other threads to release the lock whe
# they are done accessing the accelerometer readings.
def release_lock():
  accel_lock.release()