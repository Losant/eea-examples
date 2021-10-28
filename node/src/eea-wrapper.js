const { buildEEAImports, buildEEAExports } = require('./eea-api');
const { decodeSectionContent } = require('./eea-utils');

/**
 * Builds a wrapper around an EEA WASM.
 *
 * @param {Object} options - Object of options for building the wrapper.
 * @param {Buffer} options.wasmContent - The content to load as an EEA WASM.
 * @param {string} options.deviceId - The device ID.
 * @param {string} [options.persistedStoragePath] - Path to a file to read and write storage values to.
 * @param {Function} options.onMessage - Function for forwarding an eea_send_message call.
 * @param {Function} [options.registeredFunctions] - Function to call to build any user provided registered functions for the WASM.
 * @param {Integer} [options.traceLevel=1] - Used for initial value of configSetTraceLevel.
 * @param {Boolean} [options.debugEnabled=true] - Used for initial value of configSetDebugEnabled.
 * @param {Integer} [options.maxTopicSize=256] - Used for initial topic value of configSetMessageBufferLengths.
 * @param {Integer} [options.maxPayloadSize=4096] - Used for initial payload value of configSetMessageBufferLengths.
 * @param {Integer} [options.maxQueueSize=1024 * 1000] - Used for initial value of configSetQueueSize.
 * @param {Integer} [options.maxStorageSize=1024 * 1000] - Used for initial value of configSetStorageSize.
 * @param {Integer} [options.storageSaveInterval=60 * 1000] - Used for initial value of configSetStorageInterval.
 * @param {Boolean} [options.isConnected=false] - Used for initial value of setConnectionStatus.
 * @param {Integer} [options.loopInterval=1000] - Interval (milliseconds) at which to pump the EEA loop.
 *
 * @returns {Object} - Wrapper object. Functions of note are initAndStartLoop, to initialize and start
 * "pumping" the WASM, and shutdownAndStopLoop, which does the opposite.
 */
module.exports = ({
  wasmContent,
  deviceId,
  persistedStoragePath,
  onMessage,
  registeredFunctions,
  traceLevel = 1,
  debugEnabled = true,
  maxTopicSize = 256,
  maxPayloadSize = 4096,
  maxQueueSize = 1024 * 1000,
  maxStorageSize = 1024 * 1000,
  storageSaveInterval = 60 * 1000,
  isConnected = false,
  loopInterval = 1000
}) => {
  let intervalLoopId;
  const bufferInfo = {};
  const wasmMemory = new WebAssembly.Memory({ initial: 8 });
  const wasmModule = new WebAssembly.Module(wasmContent);

  const wasmInterfaceVersion = decodeSectionContent(wasmModule, 'interfaceVersion');
  const wasmBundleIdentifier = decodeSectionContent(wasmModule, 'bundleIdentifier');
  if (wasmInterfaceVersion !== '1.0.0') { throw new Error(`Interface version of ${wasmInterfaceVersion} is not supported.`); }
  if (!wasmBundleIdentifier) { throw new Error('WASM Bundle Identifier is missing.'); }

  const env = {
    ...(registeredFunctions?.(wasmMemory) || {}),
    memory: wasmMemory,
    ...buildEEAImports({
      deviceId,
      wasmMemory,
      persistedStoragePath,
      onMessage,
      bufferInfo
    })
  };

  const wasmInstance = new WebAssembly.Instance(wasmModule, { env });

  const wrapper = {
    wasmBundleIdentifier,
    wasmInterfaceVersion,
    ...buildEEAExports({ wasmMemory, wasmInstance, bufferInfo }),
    initAndStartLoop: () => {
      wrapper.init();
      intervalLoopId = setInterval(() => {
        wrapper.loop(Date.now());
      }, loopInterval);
    },
    shutdownAndStopLoop: () => {
      wrapper.shutdown();
      clearTimeout(intervalLoopId);
      intervalLoopId = null;
    }
  };

  wrapper.configSetTraceLevel(traceLevel);
  wrapper.configSetDebugEnabled(debugEnabled);
  wrapper.configSetMessageBufferLengths(maxTopicSize, maxPayloadSize);
  wrapper.configSetQueueSize(maxQueueSize);
  wrapper.configSetStorageSize(maxStorageSize);
  wrapper.configSetStorageInterval(storageSaveInterval);
  wrapper.setConnectionStatus(isConnected);

  return wrapper;
};
