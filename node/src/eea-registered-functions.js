const { encodeStringAndLength, decodeString, EEA_RESULT_CODES } = require('./eea-utils');

module.exports = (wasmMemory) => {
  return {
    eea_fn_read_accelerometer: (outPtrReadingsStr, strBufferLen, outPtrReadingsLen) => {
      const accelerometerData = '[0, 0.5, 1.5]'; // would read from real device here
      encodeStringAndLength(wasmMemory, accelerometerData, outPtrReadingsStr, strBufferLen, outPtrReadingsLen);
      return EEA_RESULT_CODES.success;
    },
    eea_fn_base64_encode: (ptrStr, strLen, outPtrEncodedStr, strBufferLen, outPtrEncodedStrLen) => {
      const input = decodeString(wasmMemory, ptrStr, strLen);
      const encoded = Buffer.from(input).toString('base64');
      encodeStringAndLength(wasmMemory, encoded, outPtrEncodedStr, strBufferLen, outPtrEncodedStrLen);
      return EEA_RESULT_CODES.success;
    }
  };
};
