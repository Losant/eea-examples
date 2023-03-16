# EEA Example Application: Rust

This folder contains an example Rust application for the [Losant Embedded Edge Agent](https://docs.losant.com/edge-compute/embedded-edge-agent/overview/) (EEA). The EEA works by compiling workflows to a [WebAssembly](https://webassembly.org/) module and publishing that module to your application over MQTT. Therefore, the majority of this code demonstrates how to load and interact with a WebAssembly (WASM) module using the imports and exports as defined in the [EEA API](http://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/).

This example is meant to be used as a starting point and reference implementation for your own Rust edge applications. It is not intended to function as an out-of-the-box EEA implementation. Significant modifications will be required when implementing a production Rust edge application that utilizes the EEA.

**The example requires Rust 2021 edition.**

## Rust WebAssembly Runtime

This example demonstrates the WebAssembly runtime: [`wasmer`](https://docs.rs/wasmer/2.3.0/wasmer/). In the current configuration the application uses the [`cranelift`](https://docs.rs/wasmer-compiler-cranelift/2.3.0/wasmer_compiler_cranelift/) WASM compiler which only supports 64-bit architectures. To compile this application for 32-bit architectures use the [`LLVM`](https://docs.rs/wasmer-compiler-llvm/2.3.0/wasmer_compiler_llvm/) compiler. Help switching to the LLVM can be found within code comments within the `Cargo.toml` and `src/wasm_helpers.rs` files.

### Installing Rust and Cargo

macOS, Linux, or another Unix-like OS:
``` bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

For other operating systems see the [`Rust docs`](https://forge.rust-lang.org/infra/other-installation-methods.html).

### Compiling

Compilation options can be found within the `Cargo.toml` file.

Local testing (run from this folder):
```bash
cargo clean
cargo build
```

Release build (run from this folder):
```bash
cargo clean
cargo build --release
```

### Compiling for non-local target platforms

Run rustup to get the desired target ([`supported platforms`](https://doc.rust-lang.org/nightly/rustc/platform-support.html)).
Example build for Raspberry Pi 64-bit:
```bash
rustup target add aarch64-unknown-linux-gnu
```

This would also require the gcc compiler for aarch64, e.g. on debian-based linux:
```bash
apt install -y gcc-aarch64-linux-gnu
```

And may require creating a cargo/config file with:
```
[target.aarch64-unknown-linux-gnu]
linker = "/usr/bin/aarch64-linux-gnu-gcc"
```

Compile with the cargo build target flag:
```bash
cargo build --target=aarch64-unknown-linux-gnu
```

As mentioned above the current configuration is only for 64-bit platforms. To build for 32-bit it's required to change the compiler in the code from cranelift to LLVM.
Compilation for microcontrollers may require significant code changes, such as converting all libraries/crates to non-std versions.

## Running this Example

### Runtime Configurations

Configuration options are located within the `resources/eea_config.toml` file.
This file contains options for device authentication, file locations, MQTT connection configurations, and [`EEA WASM compilation options`](https://docs.losant.com/edge-compute/embedded-edge-agent/mqtt-specification/#topic-and-payload).

Before execution the required configuration changes are: the Losant device authentication credentials (device ID, access key, and access secret), the local wasm bundle file path, and the local storage file path:
```
eea_device_id = "1a1a1a1a1a1a1a1a1a1a1a1a"
eea_access_key = "https://docs.losant.com/devices/overview/#create-access-key"
eea_access_secret = "https://docs.losant.com/devices/overview/#create-access-key"
eea_bundle_path = "/my/absolute/path/bundle.wasm"
eea_storage_path = "my/relative/path/eeaStore.json"
```

The file path where you run your compiled executable from may require you to update the `CONFIG_PATH` within `src/configs.rs` before compilation, currently set to run from this rust folder with the execution commands below.

### Execution

Execute test file:
```bash
./target/debug/r-eea-wrapper
```

Execute release file:
```bash
./target/release/r-eea-wrapper
```

### Interactive CLI

Upon execution you can interact with the app through the terminal commands:
- *info* - list current information about the running EEA configurations, e.g. Device ID.
- *direct* - fire a direct trigger within a local workflow. Format is `direct trigger_id JSON_payload_string`, e.g. `direct myId123 { "temp": 98.7 }`
- *exit* - clean shutdown and close application.

## Code Description

The Rust code for this example EEA app is located within the `src` folder.

Code file descriptions:

### Interactive CLI - *cli.rs*

A simple interactive CLI built with the Rust std IO crate. This is meant to run in a separate thread, and this inserts user input into a queue (`user_input_queue`).

Accepts the commands info, direct, and exit. Direct requires a direct ID and JSON payload string e.g. `direct myId123 { "temp": 98.7 }`.

### App Config / Header - *configs.rs*

Shared globals and structs.

The config file, `resources/eea_config.toml`, is lazy loaded into the `CONFIGS` object using the Rust crate one_cell.

### EEA API - *eea_api.rs*

The EEA WASM API imported functions as outlined in the docs: [`https://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#imported-functions`](https://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#imported-functions).

These functions are defined in native code and imported into the EEA WASM module. Note: although Webassembly only really supports the data types i32, i64, f32, and f64
we can declare many of them different in the Rust function arguments for casting. `WasmPtr` type and helper methods like `get_utf8_string` are all provided by the Wasmer Rust crate.

### Main - *main.rs*

EEA Rust main function.

Creates and monitors threads (MQTT, WASM, and CLI). The main loop for the main thread acts on queued messages; CLI user input, then MQTT incoming messages. Messages are passed into the running EEA WASM within here. It is also responsible for restarting the WASM runtime when a new one is passed in.

### MQTT Functions - *mqtt.rs*

MQTT helper functions which employ the Rust crate [`rumqttc`](https://docs.rs/rumqttc/0.18.0/rumqttc/).

As well as setting up the connection to the currently configured broker (`resources/eea_config.toml`) the helpers set up subscriptions, publishes messages like the [`Losant hello message`](https://docs.losant.com/edge-compute/embedded-edge-agent/mqtt-specification/#publishing-the-hello-message), and keeps track of incoming message.

TLS is used for the MQTT connection within this example, and the `mqtt_config` uses the [`rusttls crate`](https://docs.rs/rustls/latest/rustls/) to pull root certificates on your local operating system.

### Registered Functions - *registered_functions.rs*

Most of the code provided in this example applies to nearly any EEA implementation, except for the contents of `src/registered_functions.rs`.

[Registered Functions](https://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#registered-function-api) are the primary way your [Embedded Workflows](https://docs.losant.com/workflows/embedded-workflows/) interact with your application code and your device's system resources (e.g. files and attached sensors).

This code provides an example registered function so you can see how they are implemented and imported into the WASM bundle. It's unlikely that the registered function provided by this example code will apply to your use case. It is expected that you will remove the example registered functions and create your own.

### WASM Helper Functions - *wasm_helpers.rs*

WASM helper functions which employ the Rust crate [`wasmer`](https://docs.rs/wasmer/2.3.0/wasmer).

These helpers are responsible for setting up the WASM runtime and loading the current [`EEA workflow bundle`](https://docs.losant.com/edge-compute/embedded-edge-agent/overview/#how-the-eea-works). [`EEA API exported functions`](https://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#exported-functions) are also handled within this file.

---

## License

Copyright &copy; 2021 Losant IoT, Inc. All rights reserved.

Licensed under the [MIT](https://github.com/Losant/losant-examples/blob/master/LICENSE.txt) license.

https://www.losant.com