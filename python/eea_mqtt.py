#
# This file handles the MQTT communication with the Losant broker.
# For simplicity, this code is using an insecure connection.
# For a production application, using a TLS connection is recommended.
# The root CA is available at ./RootCA.cert.
#

import threading
import time

import paho.mqtt.client as mqtt

# MQTT connection details.
BROKER = "broker.losant.com"
PORT = 1883
KEEP_ALIVE = 30

def init(device_id, access_key, access_secret, eea_queue, mqtt_queue):

  def thread():

    connected = False

    # On successfull connections, subscribe to all required topics.
    def on_connect(client, userdata, flags, rc):

      if rc == 0:
        client.subscribe("losant/" + device_id + "/command")
        client.subscribe("losant/" + device_id + "/toAgent/#")

        nonlocal connected
        connected = True
        
        eea_queue.put({
          "topic": "connection_status",
          "payload": True
        })

    # On disconnection, notify the EEA by queueing a message.
    def on_disconnect(client, userdata, rc):

      print("Disconnected: " + str(rc))

      nonlocal connected
      connected = False

      eea_queue.put({
        "topic": "connection_status",
        "payload": False
      })

    # Invoked by the mqtt client whenever a message is received.
    # The only thing we need to do is queue the message so the
    # EEA can pick it up.
    def on_message(client, userdata, msg):
      print(msg)
      eea_queue.put(msg)

    def on_log(client, userdata, level, buf):
      print(str(buf))

    client = mqtt.Client(client_id=device_id)

    client.username_pw_set(username=access_key, password=access_secret)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.on_log = on_log

    # Attempt to connect to MQTT broker.
    while(True):
      try:
        print("Attempting to connect to MQTT broker...")
        client.connect(host=BROKER, port=PORT, keepalive=KEEP_ALIVE)
        break
      except:
        pass

      time.sleep(5)

    # Network loop.
    while(True):

      # If connection was lost, attempt to reconnect.
      if not connected:
        try:
          print("Attempting to reconnect to MQTT broker...")
          client.reconnect()
        except:
          pass

        time.sleep(5)

      # Check the message queue for messages to publish.
      while connected and not mqtt_queue.empty():
        message = mqtt_queue.get()
        client.publish(message["topic"], message["payload"])

      client.loop(timeout=1.0)

  # Start the thread.
  threading.Thread(target=thread, args=()).start()
