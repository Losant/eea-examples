#
# This module contains an example pywasm3 runtime.
# https://github.com/wasm3/pywasm3
#

import json
import os
import threading
import time

import wasm3

import eea_api
import eea_utils
import eea_registered_functions


# Initialize an instance of the EEA.
def init(device_id, eea_queue, mqtt_queue,
  persisted_bundle_path="./bundle-pywasm3.wasm",
  persisted_storage_path="./storage.json"):

  # These are set when the EEA calls eea_set_message_buffers.
  # These will point to pre-allocated buffers These are the buffers
  # used when sending data to the EEA for direct triggers, commands, etc.
  ptr_buffer_message_topic = None
  ptr_buffer_message_payload = None
  buffer_message_topic_length = 0
  buffer_message_payload_length = 0

  # EAA trace log level.
  # 0 = None, 1 = Errors Only, 2 = All/Verbose
  EEA_TRACE_LOG_LEVEL = 2

  # WASM3 runtime objects.
  wasm_environment = None
  wasm_runtime = None

  # The exported eea functions.
  eea_loop = None
  eea_set_connection_status = None
  eea_config_set_trace_level = None
  eea_message_received = None
  eea_shutdown = None
  eea_config_set_storage = None

  #
  # Sends the hello message to the cloud to track which bundle
  # is deployed to this device.
  #
  def send_hello_message(bundle_id):
    print("send_hello_message")

    hello_message = {
      "service": "embeddedWorkflowAgent",
      "version": "1.0.0",
      "bundle": bundle_id,
      "compilerOptions": {
        "exportMemory": True,
        "traceLevel": 2
      }
    }

    mqtt_queue.put({
      "topic": "losant/" + device_id + "/fromAgent/hello",
      "payload": json.dumps(hello_message)
    })

  # Called by the EEA to provide pre-allocated message buffers.
  # These buffers are used to send data to the EEA for 
  # direct triggers, commands, etc.
  #
  # Even though this is an EEA API function, it is not defined
  # in eea_api.py because these messages buffers are only used
  # by the EEA worker thread in this module.
  #
  def eea_set_message_buffers(buffer_topic:int, topic_buffer_len:int, buffer_payload:int, payload_buffer_len:int) -> int:
    print("eea_set_message_buffers")

    nonlocal ptr_buffer_message_topic
    nonlocal ptr_buffer_message_payload
    nonlocal buffer_message_topic_length
    nonlocal buffer_message_payload_length

    ptr_buffer_message_topic = buffer_topic
    ptr_buffer_message_payload = buffer_payload

    buffer_message_topic_length = topic_buffer_len
    buffer_message_payload_length = payload_buffer_len
    return 0

  #
  # Gets the bundle identifier from globals.
  # The identifier is encoded as two globals:
  #  BUNDLE_IDENTIFIER_LENGTH: pointer to the length, in bytes, of the bundle ID string (int).
  #  BUNDLE_IDENTIFIER: pointer to the bundle ID string.
  #
  def get_bundle_identifier(wasm_module):
    bundle_id_length_ptr = wasm_module.get_global("BUNDLE_IDENTIFIER_LENGTH")
    bundle_id_str_ptr = wasm_module.get_global("BUNDLE_IDENTIFIER")
    bundle_id_str_length = eea_utils.decode_int(wasm_runtime, 1, bundle_id_length_ptr)
    return eea_utils.decode_string(wasm_runtime, bundle_id_str_ptr, bundle_id_str_length)

  #
  # Loads a WASM module from a byte array.
  #
  def load_wasm_bundle(bundle):
    print("Loading wasm bundle...")

    # Persist bundle so it will automatically load on a restart.
    f = open(persisted_bundle_path, "wb")
    f.write(bundle)
    f.close()

    nonlocal wasm_environment
    nonlocal wasm_runtime
    nonlocal eea_loop
    nonlocal eea_set_connection_status
    nonlocal eea_config_set_trace_level
    nonlocal eea_message_received
    nonlocal eea_shutdown

    wasm_environment = wasm3.Environment()
    wasm_runtime = wasm_environment.new_runtime(32768) # 32k stack size
    
    wasm_module = wasm_environment.parse_module(bundle)
    wasm_runtime.load(wasm_module)

    reg_funcs = eea_registered_functions.init(wasm_runtime)
    eea_api_funcs = eea_api.init(device_id, wasm_runtime, mqtt_queue, persisted_storage_path)

    wasm_module.link_function("env", "eea_set_message_buffers", "i(iiii)", eea_set_message_buffers)

    # Add EEA API functions.
    for name, func in eea_api_funcs.items():
      wasm_module.link_function("env", name, func["sig"], func["func"])

    # Add the registered functions.
    for name, func in reg_funcs.items():
      wasm_module.link_function("env", name, func["sig"], func["func"])

    # Initialize the EEA.
    eea_init = wasm_runtime.find_function("eea_init")
    eea_loop = wasm_runtime.find_function("eea_loop")
    eea_set_connection_status = wasm_runtime.find_function("eea_set_connection_status")
    eea_config_set_trace_level = wasm_runtime.find_function("eea_config_set_trace_level")
    eea_message_received = wasm_runtime.find_function("eea_message_received")
    eea_shutdown = wasm_runtime.find_function("eea_shutdown")
    eea_config_set_storage_size = wasm_runtime.find_function("eea_config_set_storage_size")
    eea_config_set_storage_interval = wasm_runtime.find_function("eea_config_set_storage_interval")

    # Set EEA configuration.
    eea_config_set_trace_level(EEA_TRACE_LOG_LEVEL)
    eea_config_set_storage_size(32768)
    eea_config_set_storage_interval(30000)

    # Initialize the EEA.
    eea_init()

    # Send the Hello message to the platform so it can
    # keep track of what bundle this device has received.
    send_hello_message(get_bundle_identifier(wasm_module))

  # The main EEA worker thread.
  def thread():
    # Attempt to load a previously persisted WASM bundle.

    if os.path.exists(persisted_bundle_path):
      f = open(persisted_bundle_path, 'rb')
      bundle = f.read()
      f.close()
      load_wasm_bundle(bundle)
    else:
      send_hello_message("nullVersion")

    while(True):

      # Check the queue to see if any messages were received
      # from another thread that we need to handle.
      while not eea_queue.empty():
        message = eea_queue.get()

        # Messages that are dictionaries instead of MQTTMessage are
        # internal messages sent between threads. In this case
        # it a message sent whenever the MQTT connection status changes.
        if type(message) is dict and message["topic"] == "connection_status":
          # The MQTT connection status has changed.
          if not wasm_environment == None:
            eea_set_connection_status(message["payload"])
        elif message.topic == "losant/" + device_id + "/toAgent/flows":
          # This message is a new WASM bundle. Make sure to call eea_shutdown
          # before loading a new bundle.
          if not eea_shutdown == None:
            eea_shutdown()
          load_wasm_bundle(message.payload)
        else:
          # All other messages need to be sent into the EEA using
          # the message buffers.
          topic_length = eea_utils.encode_string(
            wasm_runtime, message.topic,
            ptr_buffer_message_topic, buffer_message_topic_length)

          message_length = eea_utils.encode_string(wasm_runtime,
            message.payload.decode("utf-8"),
            ptr_buffer_message_payload, buffer_message_payload_length)

          if not wasm_environment == None:
            eea_message_received(topic_length, message_length)
      
      # Pump the EEA.
      if not wasm_environment == None:
        eea_loop(int(time.time() * 1000))

      time.sleep(0.1)

  # Start the EEA thread.
  threading.Thread(target=thread, args=()).start()