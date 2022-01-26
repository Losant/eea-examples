# EEA Example Application: ESP32

This folder contains an example ESP32 (C++) application for the [Losant Embedded Edge Agent](https://docs.losant.com/edge-compute/embedded-edge-agent/overview/) (EEA). The EEA works by compiling workflows to a [WebAssembly](https://webassembly.org/) module and publishing that module to your application over MQTT. Therefore, the majority of this code demonstrates how to load and interact with a WebAssembly (WASM) module using the imports and exports as defined in the [EEA API](http://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/).

This example is meant to be used as a starting point and reference implementation for your own ESP32 applications. It is not intended to function as an out-of-the-box EEA implementation. Significant modifications will be required when implementing a production application.

This example is implemented using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).

## Supported ESP32 Boards

Due to the memory requirements of the EEA (~2MB), only ESP32 boards with a PSRAM chip are supported. We recommend the [Espressif ESP32 WROVER KIT](https://www.adafruit.com/product/3384).

By default, this example does not fit in the ESP32's IRAM. Please refer to Espressif's documentation on [optimizing IRAM usage](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/ram-usage.html#optimizing-iram-usage). In most cases, the following `menuconfig` settings are required:

1. Enable: Compiler options -> Optimization Level -> Optimized for size (-Os)
1. Enable: Component Config -> FreeRTOS -> Place FreeRTOS functions into Flash
1. Disable: Component Config -> Wi-Fi -> WiFi IRAM speed optimization
1. Disable: Component Config -> Wi-Fi -> WiFi RX IRAM  speed optimization

## Configuring WiFi

This example makes use of the [`example_connect()`](https://github.com/espressif/esp-idf/tree/master/examples/protocols) helper function to establish a WiFi connection. The WiFi SSID and password can be configured using `menuconfig` under the `Example Connection Configuration` section.

## Configuring the EEA

The source file `eea_config.h` contains a number of configuration options that change the behavior of this example code — the most important of which are the Losant device credentials, which are required when making an MQTT connection to Losant's broker.

## Partition Table

This example persists WASM bundles into the ESP32's [Non-Volatile Storage](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html) (NVS). By default, the ESP32's NVS is not large enough, so a new partition table is provided in `partitions.csv`. To use this table, you must change the following `menuconfig` settings:

1. Enabled: Partition Table -> Custom partition table CSV
1. Partition Table -> Custom partition table CSV file -> partitions.csv (this will be the default)

To remove the persisted WASM bundle, you can run the following command:

```
idf.py erase_flash
```

This also removes the firmware, which will require you to re-flash the ESP32 with your firmware.

## wasm3 WebAssembly Runtime

This example uses [wasm3](https://github.com/wasm3/wasm3) as the WebAssembly runtime. The build configuration of this project expects wasm3 to be in a `wasm3` folder at the same level as `main`.

```
$ cd eea-examples/esp32
$ git clone https://github.com/wasm3/wasm3.git
$ idf.py build
```

## Registered Functions

The majority of the code provided in this example applies to nearly any EEA implementation, except for the contents of `eea_registered_functions.h/cpp`.

[Registered Functions](https://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#registered-function-api) are the primary way your [Embedded Workflows](https://docs.losant.com/workflows/embedded-workflows/) interact with your application code and your device's system resources (e.g. GPIO, I2C, etc.).

This code provides registered functions that wrap several underlying [GPIO](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html) and [ADC](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html) functions for the ESP32. If your solution requires the ESP32's GPIO or ADC, you may be able to use these functions as-is. Otherwise, they are provided as examples that can help guide the implementation of your own registered functions.

---

## License

Copyright &copy; 2022 Losant IoT, Inc. All rights reserved.

Licensed under the [MIT](https://github.com/Losant/losant-examples/blob/master/LICENSE.txt) license.

https://www.losant.com
