import threading
import queue
import time
import json
import os
import serial
import base64
import paho.mqtt.client as mqtt

from wasmer import engine, Store, ImportObject, Function, Module, Instance, Memory, MemoryType
from wasmer_compiler_cranelift import Compiler

# EAA trace log level.
# 0 = None, 1 = Errors Only, 2 = All/Verbose
EEA_TRACE_LOG_LEVEL = 2

# Device authentication details.
DEVICE_ID = "000000000000000000000000"
ACCESS_KEY = "YOUR_ACCESS_KEY"
ACCESS_SECRET = "YOUR_ACCESS_SECRET"

#
# Decodes a string from the memory buffer from the starting
# pointer and length.
#
def decode_string(memory, ptr_start, length):
  buffer = memoryview(memory.buffer)
  return str(buffer[ptr_start:(ptr_start + length)], "utf-8")

#
# Encodes a string and optionally the length of the string into the memory
# buffer. Returns the length of the string, in bytes.
#
def encode_string(memory, string, string_ptr, buffer_len, length_ptr=None, length_int_size=4):
  buffer = memory.uint8_view(offset=0)
  string_encoded = string.encode("utf-8")
  length = len(string_encoded)

  # Check for buffer overflow. Attempting to encode a string
  # that is larger than the provided buffer.
  if(length > buffer_len):
    print("Buffer not large enough to encode string. Buffer capacity: " + str(buffer_len) + " to encode: " + str(length))
    return

  buffer[string_ptr:string_ptr + length] = string_encoded

  if not length_ptr == None:
    buffer[length_ptr:length_ptr + length_int_size] = length.to_bytes(length_int_size, 'little')

  return length

#
# Encodes a 32 bit integer into memory at the provided address.
# int_size is the number of bytes required to represent the int:
#  4 = 32bit, 8 = 64bit
#
def encode_int(memory, value, int_size, out_ptr_value):
  value_buffer = memory.uint8_view(offset=out_ptr_value)
  value_buffer[0:int_size] = value.to_bytes(int_size, 'little')

#
# This thread initializes and pumps the EEA.
#
def thread_eea():

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Memory
  memory = None

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Instance
  instance = None

  # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Module
  module = None

  # These are set when the EEA calls eea_set_message_buffers.
  # These will point to pre-allocated buffers with a length specified
  # by the parameters to eea_init. These are the buffers used when
  # sending data to the EEA for direct triggers, commands, etc.
  ptr_buffer_topic = None
  ptr_buffer_payload = None

  #
  # START: EEA API
  # The functions in this section implement the EEA API.
  #
 
  #
  # Called by the EEA to provide pre-allocated message buffers.
  # These buffers are used to send data to the EEA for 
  # direct triggers, commands, etc.
  #
  def eea_set_message_buffers(buffer_topic:int, topic_buffer_len:int, buffer_payload:int, payload_buffer_len:int) -> int:
    print("eea_set_message_buffers")

    nonlocal ptr_buffer_topic
    nonlocal ptr_buffer_payload

    ptr_buffer_topic = buffer_topic
    ptr_buffer_payload = buffer_payload

    return 0

  #
  # Called by the EEA to log tracing information. This is useful
  # during development to help debug what the EEA is doing.
  #
  def eea_trace(message_ptr:int, message_length:int, level:int) -> int:
    print("eea_trace")
    print(str(level) + ": " + decode_string(memory, message_ptr, message_length))
    return 0

  #
  # Called by the EEA to send a message over MQTT.
  #
  def eea_send_message(ptr_topic:int, topic_length:int, ptr_payload:int, payload_length:int, qos:int) -> int:
    print("eea_send_message")
    print("Topic: " + decode_string(memory, ptr_topic, topic_length))
    print("Payload: " + decode_string(memory, ptr_payload, payload_length))
    return 0

  #
  # Called by the EEA to persist workflow storage values.
  #
  def eea_storage_save(ptr_values:int, values_length:int) -> int:
    print("eea_storage_save")
    return 0

  #
  # Called by the EEA to retrieve persisted workflow storage values.
  #
  def eea_storage_read(out_ptr_values:int, buffer_len:int, out_ptr_values_length:int) -> int:
    print("eea_storage_read")
    return 0

  #
  # Called by the EEA to sleep the thread running the EEA.
  #
  def eea_sleep(milliseconds:int) -> int:
    print("eea_sleep")
    time.sleep(milliseconds / 1000)
    return 0

  #
  # Called by the EEA to retrieve the current device ID.
  #
  def eea_get_device_id(out_ptr_device_id:int, buffer_len:int, out_ptr_id_len:int) -> int:
    print("eea_get_device_id")
    encode_string(memory, DEVICE_ID, out_ptr_device_id, buffer_len, out_ptr_id_len, 1)
    return 0

  #
  # Called by the EEA to retrieve the current time, in milliseconds since epoch.
  #
  def eea_get_time(out_ptr_ticks_milliseconds:int) -> int:
    print("eea_get_time")
    encode_int(memory, int(time.time() * 1000), 8, out_ptr_ticks_milliseconds)
    return 0

  #
  # END: EEA API
  #

  #
  # BEGIN: Registered Functions
  #

  #
  # Retrieves a single accelerometer reading.
  #
  def eea_fn_read_accelerometer(
    out_ptr_readings_str:int,
    str_buffer_len:int,
    out_ptr_readings_len:int) -> int:

    print("eea_fn_read_accelerometer")

    encode_string(
      memory, "[0, 1, 1.5]", out_ptr_readings_str,
        str_buffer_len, out_ptr_readings_len)
    
    return 0

  #
  # END: Registered Functions
  #

  #
  # Loads a wasm bundle.
  #
  def load_wasm_bundle(bundle):
    print("Loading wasm bundle...")

    nonlocal memory
    nonlocal instance
    nonlocal module

    # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Store
    store = Store(engine.JIT(Compiler))

    # https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.MemoryType
    memory_type  = MemoryType(minimum=5, shared=False)

    memory = Memory(store, memory_type)
    module = Module(store, bundle)

    import_object = ImportObject()
    import_object.register(
      "env",
      {
        "memory": memory,
        # EEA API Functions
        "eea_trace": Function(store, eea_trace),
        "eea_set_message_buffers": Function(store, eea_set_message_buffers),
        "eea_send_message": Function(store, eea_send_message),
        "eea_storage_save": Function(store, eea_storage_save),
        "eea_storage_read": Function(store, eea_storage_read),
        "eea_get_device_id": Function(store, eea_get_device_id),
        "eea_get_time": Function(store, eea_get_time),
        "eea_sleep": Function(store, eea_sleep),
        # Registered Functions
        "eea_fn_read_accelerometer": Function(store, eea_fn_read_accelerometer)
      }
    )

    # Now the module is compiled, we can instantiate it.
    instance = Instance(module, import_object)
    instance.exports.eea_config_set_trace_level(EEA_TRACE_LOG_LEVEL)
    instance.exports.eea_init()


  # Load the EEA API wasm module from disk.
  load_wasm_bundle(open("./eea-api-memory-import.wasm", 'rb').read())

  while(True):

    if not instance == None:
      instance.exports.eea_loop(int(time.time() * 1000))

    time.sleep(1)

#
# Main entry point for the Python script.
# Start a thread for the EEA.
#
if __name__ == "__main__":

  eea_thread = threading.Thread(target=thread_eea)
  eea_thread.start()