const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder('utf8');

const scalarTypeToTypedArray = {
  bool: Uint8Array,
  i8: Int8Array,
  u8: Uint8Array,
  i16: Int16Array,
  u16: Uint16Array,
  i32: Int32Array,
  u32: Uint32Array,
  f32: Float32Array,
  i64: BigInt64Array,
  u64: BigUint64Array,
  f64: Float64Array
};

const toBigInt = (v) => BigInt(isFinite(v) ? v : 0);
const toNumber = (v) => Number(isFinite(v) ? v : 0);
const scalarCastMap = {
  bool: (v) => Number(!!v),
  i8: toNumber,
  u8: toNumber,
  i16: toNumber,
  u16: toNumber,
  i32: toNumber,
  u32: toNumber,
  f32: toNumber,
  i64: toBigInt,
  u64: toBigInt,
  f64: toBigInt
};

const EEA_RESULT_CODES = {
  success: 0,
  generalError: 1
};

/**
 * Decodes a UTF-8 string encoded in a custom section of a WASM module
 *
 * @param {WebAssembly.Module} module - The web assembly module instance
 * @param {String} sectionName - The section name to find and decode
 *
 * @returns {String} - The decoded string content of that custom section
 */
const decodeSectionContent = (module, sectionName) => {
  const sections = WebAssembly.Module.customSections(module, sectionName);
  return sections.length ? textDecoder.decode(sections[0]) : undefined;
};

/**
 * Decodes a UTF-8 string encoded in a WASM memory buffer
 *
 * @param {WebAssembly.Memory} wasmMemory - A web assembly memory instance
 * @param {Integer} ptrStart - Pointer to where in memory the string starts
 * @param {Integer} length - The length of the string, in bytes
 *
 * @returns {String} - The decoded UTF-8 string
 */
const decodeString = (wasmMemory, ptrStart, length) => {
  const bytes = new Uint8Array(wasmMemory.buffer, ptrStart, length);
  return textDecoder.decode(bytes);
};

/**
 * Encodes a UTF-8 string into a WASM memory buffer
 *
 * @param {WebAssembly.Memory} wasmMemory - A web assembly memory instance
 * @param {String} string - The string to encode
 * @param {Integer} stringPtr - Pointer to where in memory to start writing the encoded string
 * @param {Integer} bufferLength - Maximum length in bytes of the available buffer
 *
 * @returns {Integer} - The number of bytes written
 */
const encodeString = (wasmMemory, string, stringPtr, bufferLength) => {
  string = String(string);
  const outBuffer = new Uint8Array(wasmMemory.buffer, stringPtr, bufferLength);
  const encoded = textEncoder.encodeInto(string, outBuffer);
  if (encoded.read < string.length) {
    console.error(`Buffer not large enough to encode string. Buffer capacity: ${bufferLength}, to encode: ${string}`);
  }

  return encoded.written;
};

/**
 * Decodes a scalar value from a WASM memory buffer
 *
 * @param {WebAssembly.Memory} wasmMemory - A web assembly memory instance
 * @param {Integer} ptr - Pointer to where in memory to read the scalar value
 * @param {String} scalarType - The particular type of scalar. Can be one of:
 *                              bool, i8, u8, i16, u16, i32, u32, f32, i64, u64, f64
 *
 * @returns {Number|Boolean|BigInt} - The decoded scalar value
 */
const decodeScalar = (wasmMemory, ptr, scalarType) => {
  const arrayClass = scalarTypeToTypedArray[scalarType];
  if (!arrayClass) { throw new Error(`Unknown scalar type: ${scalarType}`); }
  const inBuffer = new arrayClass(wasmMemory.buffer, ptr, 1);

  return scalarType === 'bool' ? Boolean(inBuffer[0]) : inBuffer[0];
};

/**
 * Encodes a scalar value into a WASM memory buffer
 *
 * @param {WebAssembly.Memory} wasmMemory - A web assembly memory instance
 * @param {Number|Boolean|BigInt} scalarValue - The scalar value
 * @param {Integer} ptr - Pointer to where in memory to write the scalar value
 * @param {String} scalarType - The particular type of scalar. Can be one of:
 *                              bool, i8, u8, i16, u16, i32, u32, f32, i64, u64, f64
 *
 * @returns {undefined}
 */
const encodeScalar = (wasmMemory, scalarValue, ptr, scalarType) => {
  const arrayClass = scalarTypeToTypedArray[scalarType];
  if (!arrayClass) { throw new Error(`Unknown scalar type: ${scalarType}`); }
  const outBuffer = new arrayClass(wasmMemory.buffer, ptr, 1);
  outBuffer[0] = scalarCastMap[scalarType](scalarValue);
};

/**
 * Decodes an array of scalar values from a WASM memory buffer
 *
 * @param {WebAssembly.Memory} wasmMemory - A web assembly memory instance
 * @param {Integer} ptr - Pointer to where in memory to read the array of scalar values
 * @param {Integer} length - Number of items in the array to decode
 * @param {String} scalarType - The particular type of scalar. Can be one of:
 *                              bool, i8, u8, i16, u16, i32, u32, f32, i64, u64, f64
 *
 * @returns {Array<Number|Boolean|BigInt>} - The array of decoded scalar values
 */
const decodeArray = (wasmMemory, ptr, length, scalarType) => {
  const arrayClass = scalarTypeToTypedArray[scalarType];
  if (!arrayClass) { throw new Error(`Unknown scalar type: ${scalarType}`); }
  const inBuffer = new arrayClass(wasmMemory.buffer, ptr, length);

  if (scalarType === 'bool') {
    return Array.from(inBuffer, (v) => Boolean(v));
  } else {
    return Array.from(inBuffer);
  }
};

/**
 * Encodes an array of scalar values into a WASM memory buffer
 *
 * @param {WebAssembly.Memory} wasmMemory - A web assembly memory instance
 * @param {Array<Number|Boolean|BigInt>} array - The array of scalar values
 * @param {Integer} ptr - Pointer to where in memory to write the array
 * @param {Integer} maxLength - Maximum number of items that can be encoded
 * @param {String} scalarType - The particular type of scalar. Can be one of:
 *                              bool, i8, u8, i16, u16, i32, u32, f32, i64, u64, f64
 *
 * @returns {Integer} - The number of items written
 */
const encodeArray = (wasmMemory, array, ptr, maxLength, scalarType) => {
  const arrayClass = scalarTypeToTypedArray[scalarType];
  if (!arrayClass) { throw new Error(`Unknown number type: ${scalarType}`); }
  const castFunc = scalarCastMap[scalarType];

  if (array.length > maxLength) {
    console.error(`Buffer not large enough to encode array. Buffer capacity: ${maxLength}, to encode: ${array.length}`);
  }
  const length = Math.min(array.length, maxLength);

  const outBuffer = new arrayClass(wasmMemory.buffer, ptr, length);
  for (let i=0; i<length; i++) {
    outBuffer[i] = castFunc(array[i]);
  }

  return length;
};

/**
 * Encodes a UTF-8 string into a WASM memory buffer,
 * and writes the encoded byte length into the memory buffer.
 *
 * @param {WebAssembly.Memory} wasmMemory - A web assembly memory instance
 * @param {String} string - The string to encode
 * @param {Integer} stringPtr - Pointer to where in memory to start writing the encoded string
 * @param {Integer} bufferLength - Maximum length in bytes of the available buffer
 * @param {Integer} lengthPtr - Pointer to where in memory to write the encoded byte length
 * @param {String} lengthType - Type of number to write as the encoded byte length. Defaults to u32.
 *
 * @returns {undefined}
 */
const encodeStringAndLength = (wasmMemory, string, stringPtr, bufferLength, lengthPtr, lengthType = 'u32') => {
  const written = encodeString(wasmMemory, string, stringPtr, bufferLength);
  encodeScalar(wasmMemory, written, lengthPtr, lengthType);
};

module.exports = {
  EEA_RESULT_CODES,
  decodeSectionContent,
  decodeString,
  encodeString,
  encodeStringAndLength,
  decodeScalar,
  encodeScalar,
  decodeArray,
  encodeArray
};
