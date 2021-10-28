const fs = require('fs-extra');
const {
  EEA_RESULT_CODES,
  decodeString,
  encodeStringAndLength,
  encodeScalar,
  encodeString
} = require('./eea-utils');


/**
 * Builds the expected imports for an EEA WASM instance. While the implementation of some of these
 * functions will remain identical for any Javascript implementation of an EEA wrapper, there are several
 * that may need customization depending on the given use case - for example, eea_storage_read and
 * eea_storage_save would change depending on how storage should be saved.
 *
 * @param {Object} options - Object of options for building the imports.
 * @param {string} options.deviceId - The device ID. Used for eea_get_device_id.
 * @param {WebAssembly.Memory} options.wasmMemory - A web assembly memory instance.
 * @param {Function} options.onMessage - Function for forwarding an eea_send_message call.
 * @param {string} [options.persistedStoragePath] - Path to a file to read and write storage values to.
 * @param {Object} options.bufferInfo - An object in which to store the buffer pointer information obtained from an eea_set_message_buffers call.
 *
 * @returns {Object} - Object of the imports expected for an EEA WASM instance.
 */
const buildEEAImports = ({ deviceId, wasmMemory, onMessage, persistedStoragePath, bufferInfo }) => {
  return {
    /**
     * Invoked by the EEA to obtain the current Losant device ID. This is invoked as
     * part of the initialization process when eea_init is called.
     *
     * @param {Integer} ptrOutId - Pointer to the pre-allocated buffer on which to place the UTF-8 encoded device ID string.
     * @param {Integer} bufferLength - The length, in bytes, of the pre-allocated buffer.
     * @param {Integer} ptrOutIdLength - Pointer to a u8 to write the number of bytes written to the buffer.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_get_device_id: (ptrOutId, bufferLength, ptrOutIdLength) => {
      encodeStringAndLength(wasmMemory, deviceId, ptrOutId, bufferLength, ptrOutIdLength, 'u8');
      return EEA_RESULT_CODES.success;
    },


    /**
     * Invoked by the EEA to retrieve the current system time since Epoch.
     * If your system does not support time since Epoch, you must provide 0.
     *
     * @param {Integer} outTicksMilliseconds - The milliseconds of time since Epoch. If time since Epoch is not available, set this value to 0.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_get_time: (outTicksMilliseconds) => {
      encodeScalar(wasmMemory, Date.now(), outTicksMilliseconds, 'u64');
      return EEA_RESULT_CODES.success;
    },

    /**
     * Invoked by the EEA whenever a message needs to be sent to the Losant platform. This is
     * primarily done whenever an MQTT, Device State, or Debug Node is executed. Every message
     * includes a topic, payload, and quality of service (QoS) value. If your application is
     * connected to Losant’s MQTT broker, these messages can be directly published, as-is,
     * using your MQTT client.
     *
     * @param {Integer} ptrTopic - Pointer to UTF-8 encoded string.
     * @param {Integer} topicLength - The length of the string, in bytes.
     * @param {Integer} ptrPayload - The level of the current trace message.
     * @param {Integer} payloadLength - The level of the current trace message.
     * @param {Integer} qos - The level of the current trace message.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_send_message: (ptrTopic, topicLength, ptrPayload, payloadLength, qos) => {
      onMessage({
        topic: decodeString(wasmMemory, ptrTopic, topicLength),
        payload: decodeString(wasmMemory, ptrPayload, payloadLength),
        qos
      });
      return EEA_RESULT_CODES.success;
    },

    /**
     * Invoked by the EEA to provide pre-allocated buffers that are used when passing data from
     * native code to the EEA by invoking eea_message_received and eea_direct_trigger.
     *
     * @param {Integer} ptrTopicBuffer - Pre-allocated buffer to hold a message topic UTF-8 string.
     * @param {Integer} topicBufferLength - Length, in bytes, of the topic buffer.
     * @param {Integer} ptrPayloadBuffer - Pre-allocated buffer to hold a message payload UTF-8 string.
     * @param {Integer} payloadBufferLength - Length, in bytes, of the message buffer.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_set_message_buffers: (ptrTopicBuffer, topicBufferLength, ptrPayloadBuffer, payloadBufferLength) => {
      Object.assign(bufferInfo, { ptrTopicBuffer, topicBufferLength, ptrPayloadBuffer, payloadBufferLength });
      return EEA_RESULT_CODES.success;
    },

    /**
     * Invoked by the EEA to sleep the executing thread.
     * This is invoked whenever a Delay Node is executed.
     *
     * @param {Integer} milliseconds - The amount of time to sleep, in milliseconds.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_sleep: (milliseconds) => {
      const sab = new SharedArrayBuffer(4);
      const int32 = new Int32Array(sab);
      Atomics.wait(int32, 0, 0, milliseconds);
      return EEA_RESULT_CODES.success;
    },

    /**
     * Invoked by the EEA to read persisted workflow storage values. This is called by eea_init
     * whenever a new WASM bundle is initialized. If you have no persisted workflow values, your
     * native code can return 0 without populating anything in the provided buffers. This is the
     * companion function to eea_storage_save and the data you provide to the EEA should be
     * identical to the data sent to your native code.
     *
     * @param {Integer} ptrOutValues - Pointer to the pre-allocated buffer to place the UTF-8 encoded string of storage values.
     * @param {Integer} bufferLength - The length, in bytes, of the pre-allocated buffer. The value is determined by the value passed to eea_config_storage_set_size.
     * @param {Integer} ptrOutValuesLength - Pointer to a u32 to write the number of bytes written to the buffer.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_storage_read: (ptrOutValues, bufferLength, ptrOutValuesLength) => {
      if (persistedStoragePath && fs.pathExistsSync(persistedStoragePath)) {
        const storageContent = fs.readFileSync(persistedStoragePath, { encoding: 'utf8' });
        encodeStringAndLength(wasmMemory, storageContent, ptrOutValues, bufferLength, ptrOutValuesLength, 'u32');
      }
      return EEA_RESULT_CODES.success;
    },

    /**
     * Invoked by the EEA to persist all workflow storage values. This function will be invoked
     * on an interval defined by eea_config_set_storage_interval and whenever eea_shutdown
     * is called (if the configured interval is not 0). The values are provided as a JSON string
     * and it’s up to your native code to determine how to save the data (e.g. file or flash storage).
     *
     * @param {Integer} ptrValues - Pointer to UTF-8 encoded string.
     * @param {Integer} valuesLength - The length, in bytes, of the values string.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_storage_save: (ptrValues, valuesLength) => {
      if (persistedStoragePath) {
        const storageContent = decodeString(wasmMemory, ptrValues, valuesLength);
        fs.writeFileSync(persistedStoragePath, storageContent);
      }
      return EEA_RESULT_CODES.success;
    },

    /**
     * Invoked by the EEA to log tracing information. This is useful for early development or to
     * debug unexpected behavior. The most common practice is to write these messages to a
     * console or standard output. Tracing can be dynamically enabled or disabled by
     * invoking eea_config_set_trace_level.
     *
     * @param {Integer} messagePtr - Pointer to UTF-8 encoded string.
     * @param {Integer} messageLength - The length of the string, in bytes.
     * @param {Integer} level - The level of the current trace message.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    eea_trace: (messagePtr, messageLength, level) => {
      console.warn(`eea_trace: Level ${level} - ${decodeString(wasmMemory, messagePtr, messageLength)}`);
      return EEA_RESULT_CODES.success;
    }
  };
};

/**
 * Builds wrappers around the expected exports of an EEA WASM instance. Many of these are direct
 * pass-throughs, just renaming exports to feel more like native Javascript functions. Of note, however,
 * messageReceived and directTrigger do wrap the string encoding and pointer manipulation required of the
 * underlying call, allowing easier Javascript usage of those functions.
 *
 * @param {Object} options - Object of options for building the exports.
 * @param {WebAssembly.Memory} options.wasmMemory - A web assembly memory instance.
 * @param {WebAssembly.Module} options.wasmInstance - The web assembly module instance.
 * @param {Object} options.bufferInfo - An object containing the buffer pointer information (set by eea_set_message_buffers)
 *
 * @returns {Object} - Object wrapping the expected exports of an EEA WASM instance.
 */
const buildEEAExports = ({ wasmMemory, wasmInstance, bufferInfo }) => {
  return {
    /**
     * Changes the debug configuration for the EEA. This controls whether the EEA will invoke
     * eea_send_message whenever a Debug Node is invoked. You may want to disable this to reduce
     * the amount of bandwidth consumed by your device. This configuration option is constrained
     * by the disableDebugMessage compiler option sent in the Hello Message. For example, if your
     * device sent disableDebugMessage: true, attempting to enable debug messages using this
     * function will have no effect.
     *
     * @param {Boolean} enabled - Whether debug messages will be sent by the EEA. Defaults to true.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    configSetDebugEnabled: (enabled) => {
      return wasmInstance.exports.eea_config_set_debug_enabled(enabled);
    },

    /**
     * Sets the length, in bytes, of the message buffers, which are used to send data
     * from native code to the EEA.
     *
     * @param {Integer} topicBufferLength - The size, in bytes, to pre-allocate for the message buffer’s topic. Defaults to 256 bytes.
     * @param {Integer} payloadBufferLength - The size, in bytes, to pre-allocate for the message buffer’s payload. Defaults to 4096 bytes.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    configSetMessageBufferLengths: (topicBufferLength, payloadBufferLength) => {
      return wasmInstance.exports.eea_config_set_message_buffer_lengths(topicBufferLength, payloadBufferLength);
    },

    /**
     * Limits the total amount of memory the workflow queue can consume. Each call to eea_loop
     * will execute, at most, one workflow. So for example, if your native code calls
     * eea_direct_trigger or eea_message_received several times between calls to eea_loop,
     * each of those triggers and their corresponding workflows will be queued for eventual
     * execution in subsequent calls to eea_loop.
     *
     * @param {Integer} size - The maximum amount of memory, in bytes, that the workflow queue can consume. Defaults to 1024000 bytes (1024 * 1000).
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    configSetQueueSize: (size) => {
      return wasmInstance.exports.eea_config_set_queue_size(size);
    },

    /**
     * Sets the interval, in milliseconds, that the EEA will invoke eea_storage_save to
     * persist all workflow storage values. Setting this value to 0 will disable persisting
     * workflow storage and storage values will only exist in memory.
     *
     * @param {Integer} interval - The interval, in milliseconds, that the EEA will persist workflow storage. Defaults to 0 (disabled).
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    configSetStorageInterval: (interval) => {
      return wasmInstance.exports.eea_config_set_storage_interval(interval);
    },

    /**
     * Sets the maximum size, in bytes, that workflow storage is allowed to consume. This also
     * controls the size of the buffer sent to eea_storage_read when the EEA needs to read
     * persisted storage values. For embedded devices with constrained memory, you will
     * likely be required to lower this value.
     *
     * @param {Integer} size - The maximum size, in bytes, that workflow storage will consume. Defaults to 1024000 (1024 * 1000).
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    configSetStorageSize: (size) => {
      return wasmInstance.exports.eea_config_set_storage_size(size);
    },

    /**
     * Filters the trace logs by a specific level. The maximum level is constrained by the
     * traceLevel compiler option sent in the Hello Message. For example, if your device sent
     * traceLevel: 1 in the compiler options, attempting to set the trace level to 2 using this
     * function will have no effect.
     *
     * @param {Integer} level - The trace log level. Defaults to 0. Options are 0 (disabled), 1 (error), 2 (verbose).
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    configSetTraceLevel: (level) => {
      return wasmInstance.exports.eea_config_set_trace_level(level);
    },

    /**
     * Invokes one or more Direct Triggers. The payload must be a valid JSON string. Workflows can
     * contain any number of Direct Triggers. The user-specified ID can be the same for multiple triggers.
     * The trigger’s ID is defined using the workflow editor. This function invokes every Direct
     * Trigger that matches the ID provided.
     *
     * @param {string} triggerId - The triggerId to trigger.
     * @param {string} payload - The payload to trigger with (must be a valid JSON string).
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    directTrigger: (triggerId, payload) => {
      return wasmInstance.exports.eea_direct_trigger(
        encodeString(wasmMemory, triggerId, bufferInfo.ptrTopicBuffer, bufferInfo.topicBufferLength),
        encodeString(wasmMemory, payload, bufferInfo.ptrPayloadBuffer, bufferInfo.payloadBufferLength)
      );
    },

    /**
     * Initializes the EEA. Must be the first thing called after loading the EEA WASM bundle. This
     * function will invoke several imported functions as part of the initialization process:
     * eea_set_message_buffers, eea_get_device_id, and eea_storage_read.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    init: () => {
      return wasmInstance.exports.eea_init();
    },

    /**
     * The main worker loop for the EEA. This must be called continually by the native code with
     * minimal delay between invocations (< 100ms). This function requires the current system time,
     * in milliseconds, either since Epoch or since boot (whichever is available on your system).
     *
     * A call to eea_loop will execute, at most, a single workflow in each iteration. Workflows
     * are executed in the order they are queued. For example, if your native code queued a workflow
     * by invoking eea_direct_trigger, and there are no other workflows queued, the next call to
     * eea_loop will execute the workflow. That same call to eea_loop may result in a timer being
     * triggered, but since a workflow has already run on this iteration, any workflows triggered
     * from that timer will be queued for the next call to eea_loop.
     *
     * @param {Integer} ticksMilliseconds - The milliseconds of time since Epoch or since device boot.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    loop: (ticksMilliseconds) => {
      return wasmInstance.exports.eea_loop(BigInt(ticksMilliseconds));
    },

    /**
     * Forwards MQTT data to the EEA. May be used to invoke MQTT triggers, Device Command triggers,
     * or Virtual Button triggers.
     *
     * @param {string} topic - The mqtt topic.
     * @param {string} payload - The mqtt payload.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    messageReceived: (topic, payload) => {
      return wasmInstance.exports.eea_message_received(
        encodeString(wasmMemory, topic, bufferInfo.ptrTopicBuffer, bufferInfo.topicBufferLength),
        encodeString(wasmMemory, payload, bufferInfo.ptrPayloadBuffer, bufferInfo.payloadBufferLength)
      );
    },

    /**
     * Called by your native code whenever the MQTT connection status changes. This information
     * is primarily used to set the isConnectedToLosant field that is automatically added
     * to every workflow payload.
     *
     * @param {Boolean} connected - Whether the device is connected to the MQTT broker (true = connected, false = not connected).
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    setConnectionStatus: (connected) => {
      return wasmInstance.exports.eea_set_connection_status(connected);
    },

    /**
     * Called by your native code prior to destroying an EEA WASM bundle. This is primarily used
     * to invoke eea_storage_save to ensure any pending workflow storage values are persisted.
     * The most common reason to invoke eea_shutdown is when a device receives a new WASM bundle.
     * The native code must destroy the prior bundle before loading the new bundle.
     *
     * @returns {Integer} - Error code. 0 for success.
     */
    shutdown: () => {
      return wasmInstance.exports.eea_shutdown();
    }
  };
};

module.exports = {
  buildEEAImports,
  buildEEAExports
};
