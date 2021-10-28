import struct

#
# Gets a memory object based on the runtime you're using:
# wasmer or wasm3. If using a different runtime,
# this will need changed.
#
def get_memory(memory, writable):
  # This is a memory object from wasmer.
  if hasattr(memory, 'buffer'):
    if writable:
      return memory.uint8_view(offset=0)
    else:
      return memoryview(memory.buffer)
  
  # This is a M3_Runtime object from wasm3.
  return memory.get_memory(0)

#
# Decodes a string from the memory buffer from the starting
# pointer and length.
#
def decode_string(memory, ptr_start, length):
  buffer = get_memory(memory, False)
  return str(buffer[ptr_start:(ptr_start + length)], "utf-8")

#
# Encodes a string and optionally the length of the string into the memory
# buffer. Returns the length of the string, in bytes.
#
def encode_string(memory, string, string_ptr, buffer_len, length_ptr=None, length_int_size=4):
  buffer = get_memory(memory, True)
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
# Encodes an integer into memory at the provided address.
# int_size is the number of bytes required to represent the int:
#  4 = 32bit, 8 = 64bit
#
def encode_int(memory, value, int_size, out_ptr_value):
  buffer = get_memory(memory, True)
  buffer[out_ptr_value:out_ptr_value + int_size] = value.to_bytes(int_size, 'little')

# Decodes an int from memory at the provided address.
# int_size is the number of bytes required to represent the int:
#  4 = 32bit, 8 = 64bit
def decode_int(memory, int_size, ptr_value):
  buffer = get_memory(memory, False)
  int_bytes = buffer[ptr_value:ptr_value + int_size]
  return int.from_bytes(int_bytes, 'little')

#
# Encodes a 32-bit float into memory at the provided address.
#
def encode_float(memory, value, out_ptr_value):
  buffer = get_memory(memory, True)
  value_bytes = struct.pack('<f', value)
  buffer[out_ptr_value:out_ptr_value + 4] = value_bytes