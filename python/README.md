# EEA Example Application: Python

This folder contains an example Python application for the [Losant Embedded Edge Agent](https://docs.losant.com/edge-compute/embedded-edge-agent/overview/) (EEA). The EEA works by compiling workflows to a [WebAssembly](https://webassembly.org/) module and publishing that module to your application over MQTT. Therefore, the majority of this code demonstrates how to load and interact with a WebAssembly (WASM) module using the imports and exports as defined in the [EEA API](http://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/).

This example is meant to be used as a starting point and reference implementation for your own Python edge applications. It is not intended to function as an out-of-the-box EEA implementation. Significant modifications will be required when implementing a production Python edge application that utilizes the EEA.

**The example requires Python 3.**

## Python WebAssembly Runtimes

This example demonstrates two WebAssembly runtimes: [`wasmer-python`](https://github.com/wasmerio/wasmer-python) and [`pywasm3`](https://github.com/wasm3/pywasm3). Which runtime you choose depends on your environment and your personal preferences. If your environment is a [Raspberry Pi](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) with a default installation of [Raspbian](https://www.raspbian.org/), we recommend `pywasm3` since `wasmer-python` is not supported on 32-bit operating systems.

### Installing wasmer-python

```bash
pip3 install wasmer==1.0.0
pip3 install wasmer_compiler_cranelift==1.0.0
```

Refer to the [`wasmer-python` documentation](https://github.com/wasmerio/wasmer-python) for more details.

### Installing pywasm3

```bash
pip3 install pywasm3
```

Refer to the [`pywasm3` documentation](https://github.com/wasm3/pywasm3) for more details.

### Switching Between Runtimes

You can switch between runtimes by modifying `main.py` and changing which runtime is commented out:

```python
#import eea_runtime_wasmer as eea
import eea_runtime_pywasm3 as eea
```

## Other Dependencies

All EEA-to-platform communication is done using MQTT. This example makes use of the [Paho Python Client](https://www.eclipse.org/paho/index.php?page=clients/python/index.php) (`paho-mqtt`) for MQTT communication.

```bash
pip3 install paho-mqtt
```

## Running this Example
The Losant device authentication credentials (device ID, access key, and access secret) are provided to this script using environment variables:

```bash
DEVICE_ID=YOUR_DEVICE_ID \
ACCESS_KEY=YOUR_ACCESS_KEY \
ACCESS_SECRET=YOUR_ACCESS_SECRET \
python3 main.py
```

## Registered Functions

The majority of the code provided in this example applies to nearly any EEA implementation, except for the contents of `eea_registered_functions.py`.

[Registered Functions](https://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#registered-function-api) are the primary way your [Embedded Workflows](https://docs.losant.com/workflows/embedded-workflows/) interact with your application code and your device's system resources (e.g. files and attached sensors).

This code provides some example registered functions so you can see how they are implemented and imported into the WASM bundle. It's unlikely that the registered function provided by this example code will apply to your use case. It is expected that you will remove the example registered functions and create your own.

---

## License

Copyright &copy; 2025 Losant IoT, Inc. All rights reserved.

Licensed under the [MIT](https://github.com/Losant/losant-examples/blob/master/LICENSE.txt) license.

https://www.losant.com