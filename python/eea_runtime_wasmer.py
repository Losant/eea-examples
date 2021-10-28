#
# This module contains an example wasmer runtime
# https://github.com/wasmerio/wasmer-python
#

import json
import os
import threading
import time

import eea_api
import eea_utils
import eea_registered_functions

from wasmer import engine, Store, ImportObject, Function, Module, Instance, Memory, MemoryType
from wasmer_compiler_cranelift import Compiler

# Initialize an instance of the EEA.
def init(device_id, eea_queue, mqtt_queue,
  persisted_bundle_path="./bundle-wasmer.wasm",
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

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Instance
  wasm_instance = None

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Module
  wasm_module = None

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Memory
  wasm_memory = None

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Store
  wasm_store = None

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.MemoryType
  wasm_memory_type = None

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
        "traceLevel": 2
      }
    }

    mqtt_queue.put({
      "topic": "losant/" + device_id + "/fromAgent/hello",
      "payload": json.dumps(hello_message)
    })

    print(hello_message)

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
  def get_bundle_identifier(wasm_instance):
    bundle_id_length_ptr = wasm_instance.exports.BUNDLE_IDENTIFIER_LENGTH.value

    bundle_id_str_ptr = wasm_instance.exports.BUNDLE_IDENTIFIER.value
    bundle_id_str_length = eea_utils.decode_int(wasm_memory, 1, bundle_id_length_ptr)
    print(bundle_id_str_length)
    return eea_utils.decode_string(wasm_memory, bundle_id_str_ptr, bundle_id_str_length)

  #
  # Loads a WASM module from a byte array.
  #
  def load_wasm_bundle(bundle):
    print("Loading wasm bundle...")

    # Persist bundle so it will automatically load on a restart.
    f = open(persisted_bundle_path, "wb")
    f.write(bundle)
    f.close()

    nonlocal wasm_memory
    nonlocal wasm_module
    nonlocal wasm_instance
    nonlocal wasm_store
    nonlocal wasm_memory_type

    wasm_store = Store(engine.JIT(Compiler))
    wasm_memory_type = MemoryType(minimum=5, shared=False)
    wasm_memory = Memory(wasm_store, wasm_memory_type)
    wasm_module = Module(wasm_store, bundle)

    reg_funcs = eea_registered_functions.init(wasm_memory)
    eea_api_funcs = eea_api.init(device_id, wasm_memory, mqtt_queue, persisted_storage_path)

    imports = {
      "memory": wasm_memory,
      "eea_set_message_buffers": Function(wasm_store, eea_set_message_buffers)
    }

    # Add EEA API functions.
    for name, func in eea_api_funcs.items():
      imports[name] = Function(wasm_store, func["func"])

    # Add the registered functions.
    for name, func in reg_funcs.items():
      imports[name] = Function(wasm_store, func["func"])

    # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.ImportObject
    import_object = ImportObject()
    import_object.register(
      "env",
      imports
    )

    # Now the module is compiled, we can instantiate it.
    wasm_instance = Instance(wasm_module, import_object)

    # Set EEA configuration.
    wasm_instance.exports.eea_config_set_trace_level(EEA_TRACE_LOG_LEVEL)
    wasm_instance.exports.eea_config_set_storage_size(32768)
    wasm_instance.exports.eea_config_set_storage_interval(30000)

    # Initialize the EEA.
    wasm_instance.exports.eea_init()

    # Send the Hello message to the platform so it can
    # keep track of what bundle this device has received.
    send_hello_message(get_bundle_identifier(wasm_instance))

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
          if not wasm_instance == None:
            wasm_instance.exports.eea_set_connection_status(message["payload"])
        elif message.topic == "losant/" + device_id + "/toAgent/flows":
          # This message is a new WASM bundle. Make sure to call eea_shutdown
          # before loading a new bundle.
          if not wasm_instance == None:
            wasm_instance.exports.eea_shutdown()

          load_wasm_bundle(message.payload)
        else:
          # All other messages need to be sent into the EEA using
          # the message buffers.
          topic_length = eea_utils.encode_string(
            wasm_memory, message.topic,
            ptr_buffer_message_topic, buffer_message_topic_length)

          message_length = eea_utils.encode_string(wasm_memory,
            message.payload.decode("utf-8"),
            ptr_buffer_message_payload, buffer_message_payload_length)

          if not wasm_instance == None:
            wasm_instance.exports.eea_message_received(topic_length, message_length)
      
      # Pump the EEA.
      if not wasm_instance == None:
        wasm_instance.exports.eea_loop(int(time.time() * 1000))

      time.sleep(0.1)

  # Start the EEA thread.
  threading.Thread(target=thread, args=()).start()