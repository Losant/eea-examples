const mqtt = require('mqtt');
const fs = require('fs-extra');
const buildEEAWrapper = require('./eea-wrapper');

/**
 * Builds a wrapper around an EEA WASM, with MQTT connectivity. Will automatically attempt to
 * connect and load the WASM.
 *
 * @param {Object} options - Object of options for building the wrapper.
 * @param {string} options.deviceId - The device ID.
 * @param {string} options.accessKey - The Access Key to use to connect to the MQTT broker.
 * @param {string} options.accessSecret - The Access Secret to use to the MQTT broker.
 * @param {string} [options.brokerHost='broker.losant.com'] - The hostname of the broker.
 * @param {string} [options.brokerProtocol='mqtts://'] - The protocol to use to connect to the broker.
 * @param {string} [options.persistedWasmPath] - Path to a file to persist the WASM to.
 * @param {...*} options.eeaWrapperOptions - Other options to pass through to the EEA Wrapper.
 *
 * @returns {Object} - Wrapper object with functions to access the MQTT client, the EEA WASM wrapper, and to shut down both.
 *
 */
module.exports = async ({
  deviceId,
  accessKey,
  accessSecret,
  brokerProtocol = 'mqtts://',
  brokerHost = 'broker.losant.com',
  persistedWasmPath,
  ...eeaWrapperOptions
}) => {
  let eeaWrapper;

  const client = mqtt.connect(`${brokerProtocol}${brokerHost}`, {
    clientId: deviceId,
    username: accessKey,
    password: accessSecret,
    queueQoSZero: false,
    clean: true,
    resubscribe: false
  });

  const sendHelloMessage = () => {
    const helloMessage = {
      service: 'embeddedWorkflowAgent',
      version: '1.0.0',
      bundle:  eeaWrapper?.wasmBundleIdentifier || 'nullVersion',
      compilerOptions: { traceLevel: 2 }
    };

    client.publish(`losant/${deviceId}/fromAgent/hello`, JSON.stringify(helloMessage), { qos: 0 });
  };

  const loadNewWasm = async (newWasmContent) => {
    let newWasmInstance;
    try {
      newWasmInstance = buildEEAWrapper({
        ...eeaWrapperOptions,
        wasmContent: newWasmContent,
        deviceId,
        isConnected: client.connected,
        onMessage: ({ topic, payload, qos = 0 }) => {
          client.publish(topic, payload, { qos });
        }
      });
    } catch (e) {
      console.error(`Error loading wasm: ${e}`);
      return;
    }

    if (eeaWrapper) {
      eeaWrapper.shutdownAndStopLoop();
    }

    eeaWrapper = newWasmInstance;
    eeaWrapper.initAndStartLoop();
    sendHelloMessage();

    if (persistedWasmPath) {
      await fs.writeFile(persistedWasmPath, newWasmContent);
    }
  };

  client.on('close', () => eeaWrapper?.setConnectionStatus(false));
  client.on('offline', () => eeaWrapper?.setConnectionStatus(false));

  client.on('connect', async () => {
    eeaWrapper?.setConnectionStatus(true);

    await client.subscribe([
      `losant/${deviceId}/toAgent/#`,
      `losant/${deviceId}/command`,
      'foo',
      'bar'
    ], { qos: 1 });

    sendHelloMessage();
  });

  client.on('message', (topic, message) => {
    if (topic === `losant/${deviceId}/toAgent/flows`) {
      loadNewWasm(message);
    } else {
      eeaWrapper?.messageReceived(topic, message);
    }
  });

  if (persistedWasmPath && await fs.pathExists(persistedWasmPath)) {
    const wasmContent = await fs.readFile(persistedWasmPath);
    await loadNewWasm(wasmContent);
  }

  return {
    eeaWrapper: () => eeaWrapper,
    client: () => client,
    shutdown: () => {
      if (eeaWrapper) {
        eeaWrapper.shutdownAndStopLoop();
        eeaWrapper = null;
      }

      client.end();
    }
  };
};
