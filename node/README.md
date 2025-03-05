# EEA Example Application: Node.js

This folder contains an example Node.js application for the [Losant Embedded Edge Agent](https://docs.losant.com/edge-compute/embedded-edge-agent/overview/) (EEA). The EEA works by compiling workflows to a [WebAssembly](https://webassembly.org/) module and publishing that module to your application over MQTT. Therefore, the majority of this code demonstrates how to load and interact with a WebAssembly (WASM) module using the imports and exports as defined in the [EEA API](http://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/).

This example is meant to be used as a starting point and reference implementation for your own Node.js edge applications. It is not intended to function as an out-of-the-box EEA implementation. Significant modifications will be required when implementing a production Node.js edge application that utilizes the EEA.

**The example requires Node.js 16 or higher.** It is also possible to run this example in Node.js 14 with the Node.js flag `--experimental-wasm-bigint`.

## Dependencies

All EEA-to-platform communication is done using MQTT. This example makes use of the [`mqtt`](https://www.npmjs.com/package/mqtt) npm package for MQTT communication. This example also uses [`fs-extra`](https://www.npmjs.com/package/fs-extra) for some file system operations, but this dependency is not required in all cases.

## Running this Example

The Losant device authentication credentials (device ID, access key, and access secret) are provided to this script using environment variables:

```bash
DEVICE_ID=YOUR_DEVICE_ID \
ACCESS_KEY=YOUR_ACCESS_KEY \
ACCESS_SECRET=YOUR_ACCESS_SECRET \
./bin/cli.js
```

## Registered Functions

The majority of the code provided in this example applies to nearly any EEA implementation, except for the contents of `eea-registered-functions.js`.

[Registered Functions](https://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#registered-function-api) are the primary way your [Embedded Workflows](https://docs.losant.com/workflows/embedded-workflows/) interact with your application code and your device's system resources (e.g. files and attached sensors).

This code provides some example registered functions so you can see how they are implemented and imported into the WASM bundle. It's unlikely that the registered function provided by this example code will apply to your use case. It is expected that you will remove the example registered functions and create your own.

---

## License

Copyright &copy; 2025 Losant IoT, Inc. All rights reserved.

Licensed under the [MIT](https://github.com/Losant/losant-examples/blob/master/LICENSE.txt) license.
