import time
import os

import eea_utils

def init(device_id, wasm_memory, mqtt_queue, persisted_storage_path):

  #
  # Called by the EEA to log tracing information. This is useful
  # during development to help debug what the EEA is doing.
  #
  def eea_trace(message_ptr:int, message_length:int, level:int) -> int:
    print("eea_trace")
    print(str(level) + ": " + eea_utils.decode_string(wasm_memory, message_ptr, message_length))
    return 0

  #
  # Called by the EEA to send a message over MQTT.
  #
  def eea_send_message(ptr_topic:int, topic_length:int, ptr_payload:int, payload_length:int, qos:int) -> int:
    print("eea_send_message")

    mqtt_queue.put({
      "topic": eea_utils.decode_string(wasm_memory, ptr_topic, topic_length),
      "payload": eea_utils.decode_string(wasm_memory, ptr_payload, payload_length),
      "qos": qos
    })

    return 0

  #
  # Called by the EEA to persist workflow storage values.
  #
  def eea_storage_save(ptr_values:int, values_length:int) -> int:
    print("eea_storage_save")
    if not persisted_storage_path == None:
      f = open(persisted_storage_path, 'w')
      f.write(eea_utils.decode_string(wasm_memory, ptr_values, values_length))
      f.close()
    return 0

  #
  # Called by the EEA to retrieve persisted workflow storage values.
  #
  def eea_storage_read(out_ptr_values:int, buffer_len:int, out_ptr_values_length:int) -> int:
    print("eea_storage_read")
    if not persisted_storage_path == None and os.path.exists(persisted_storage_path):

      f = open(persisted_storage_path, 'r')
      values = f.read()
      f.close()

      if len(values) > 0:
        eea_utils.encode_string(wasm_memory, values,
          out_ptr_values, buffer_len, out_ptr_values_length)

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
    eea_utils.encode_string(wasm_memory, device_id, out_ptr_device_id, buffer_len, out_ptr_id_len, 1)
    return 0

  #
  # Called by the EEA to retrieve the current time, in milliseconds since epoch.
  #
  def eea_get_time(out_ptr_ticks_milliseconds:int) -> int:
    print("eea_get_time")
    eea_utils.encode_int(wasm_memory, int(time.time() * 1000), 8, out_ptr_ticks_milliseconds)
    return 0

  # Return a dictionary of import names and functions.
  # The "sig" values are required when using pywasm3. They are ignored for wasmer.
  return {
    "eea_trace":          { "sig": "i(iii)",    "func": eea_trace },
    "eea_send_message":   { "sig": "i(iiiii)",  "func": eea_send_message },
    "eea_storage_save":   { "sig": "i(ii)",     "func": eea_storage_save },
    "eea_storage_read":   { "sig": "i(iii)",    "func": eea_storage_read },
    "eea_sleep":          { "sig": "i(i)",      "func": eea_sleep },
    "eea_get_device_id":  { "sig": "i(iii)",    "func": eea_get_device_id },
    "eea_get_time":       { "sig": "i(i)",      "func": eea_get_time }
  }