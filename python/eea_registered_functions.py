#
# This module contains any registered functions. The functions and their
# imported names are returned as a dictionary at the bottom of this file.
# The code in eea_wasmer and eea_pywasm3 automatically imports from
# whatever is specified in this dictionary.
#
# You will have to modify this file for your use case.
#

import base64
import os

import eea_utils
import eea_accelerometer

def init(wasm_memory):

  PUMP_STATUS_PATH = "../losant-edge-agent/data/pump_status.txt"

  def eea_fn_custom_trace(ptr_str:int, str_len:int) -> int:
    message = eea_utils.decode_string(wasm_memory, ptr_str, str_len)
    print(message)
    return 0

  #
  # Outputs the most recently read accelerometer data.
  #
  def eea_fn_read_accelerometer(out_ptr_readings_str:int, str_buffer_len:int, out_ptr_readings_len:int) -> int:
    print("eea_fn_read_accelerometer")

    eea_utils.encode_string(
        wasm_memory, "[0, 0.5, 1.5]",
        out_ptr_readings_str, str_buffer_len, out_ptr_readings_len)

    #eea_accelerometer.acquire_lock()

    #if not eea_accelerometer.accel_readings == None:
    #  eea_utils.encode_string(
    #    wasm_memory, eea_accelerometer.accel_readings,
    #    out_ptr_readings_str, str_buffer_len, out_ptr_readings_len)
    #  eea_accelerometer.release_lock()
    #else:
    #  eea_accelerometer.release_lock()
    #  return 1

    return 0

  #
  # Encodes a string to base64.
  #
  def eea_fn_base64_encode(ptr_str:int, str_len:int, out_ptr_encoded_str:int, str_buffer_len:int, out_ptr_encoded_str_len:int) -> int:
    print("eea_fn_base64_encode")
    input = eea_utils.decode_string(wasm_memory, ptr_str, str_len)
    encoded = base64.b64encode(input.encode("utf-8")).decode("utf-8")
    eea_utils.encode_string(wasm_memory, encoded, out_ptr_encoded_str, str_buffer_len, out_ptr_encoded_str_len)
    
    return 0

  #
  # Get pump status by reading a 1/0 from PUMP_STATUS_PATH.
  #
  def eea_fn_get_pump_status(out_ptr_status:int) -> int:
    if os.path.exists(PUMP_STATUS_PATH):
      result = int(open(PUMP_STATUS_PATH, 'rb').read().decode("utf-8"))
      eea_utils.encode_int(wasm_memory, result, 4, out_ptr_status)
    else:
      eea_utils.encode_int(wasm_memory, 0, 4, out_ptr_status)
    return 0

  # Return a dictionary of import names and functions.
  # The "sig" values are required when using pywasm3. They are ignored for wasmer.
  return {
    #"eea_fn_custom_trace":        { "sig": "i(ii)",     "func": eea_fn_custom_trace },
    #"eea_fn_read_accelerometer":  { "sig": "i(iii)",    "func": eea_fn_read_accelerometer },
    #"eea_fn_base64_encode":       { "sig": "i(iiiii)",  "func": eea_fn_base64_encode },
    #"eea_fn_get_pump_status":     { "sig": "i(i)",      "func": eea_fn_get_pump_status },
  }