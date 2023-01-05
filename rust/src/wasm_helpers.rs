/*
    EEA WASM helper functions
*/
use crate::configs::{
    DEFAULT_BUNDLE_ID, CONFIGS, Env, WasmInfo, PTR_BUFFER_MESSAGE_TOPIC, PTR_BUFFER_MESSAGE_PAYLOAD
};
use crate::eea_api;
use crate::registered_functions;

use rumqttc::Client;
use wasmer::{
    Universal, Store, Module, Instance, MemoryType, Memory,
    MemoryView, imports, Function, NativeFunc
};

// use wasmer_compiler_llvm::LLVM; // 32-bit architectures require the LLVM compiler

use wasmer_compiler_cranelift::Cranelift; // does not support 32-bit architectures
use std::io::Read;
use libflate::gzip::Decoder;
use std::sync::atomic::Ordering;
use std::path::Path;
use std::fs::read;
use std::error::Error;
use std::process::exit;
use std::str::from_utf8;

// EEA Bundle ID must be pulled from the currently running WASM
fn get_bundle_id(wasm_memory: Memory, bundle_id_str_ptr: i32, bundle_id_length_ptr: i32) -> String {
    let view: MemoryView<u8> = wasm_memory.view();
    let bundle_id_length = view[(bundle_id_length_ptr) as usize].get();
    let str_vec: Vec<_> = view[bundle_id_str_ptr as usize..(bundle_id_str_ptr as usize + bundle_id_length as usize)]
        .iter()
        .map(|cell| cell.get())
        .collect();

    // Convert the subslice to a `String`... trusting it will work
    from_utf8(&str_vec).unwrap().to_owned()
}

// EEA WASM configure options 
fn configure_eea_wasm(wasm_instance: Instance) -> Result<i32, Box<dyn Error>> {
    // configure trace
    let eea_config_set_trace_level: NativeFunc<i32, i32> = wasm_instance.exports.get_native_function("eea_config_set_trace_level")?;
    eea_config_set_trace_level.call(CONFIGS.eea_trace_topic_level)?;

    // configure storage
    let eea_config_set_storage_size: NativeFunc<i32, i32> = wasm_instance.exports.get_native_function("eea_config_set_storage_size")?;
    eea_config_set_storage_size.call(CONFIGS.eea_storage_size)?;
    let eea_config_set_storage_interval: NativeFunc<i32, i32> = wasm_instance.exports.get_native_function("eea_config_set_storage_interval")?;
    Ok(eea_config_set_storage_interval.call(CONFIGS.eea_storage_interval)?)

    /*  available options not used in this example:
        let eea_config_set_debug_enabled: NativeFunc<i32, i32> = wasm_instance.exports.get_native_function("eea_config_set_debug_enabled")
            .unwrap(),
        let eea_config_set_message_buffer_lengths: NativeFunc<i32, i32> = wasm_instance.exports.get_native_function("eea_config_set_message_buffer_lengths")
            .unwrap(),
    */
}

/*
    Called by the EEA to provide pre-allocated message buffers.
    These buffers are used to send data to the EEA for 
    direct triggers, commands, etc.

    Encodes strings into memory, and then return lengths for topic and payload
*/
pub fn set_message_buffer(wasm_memory: &Memory, payload: String, topic: String) -> (i32, i32) {
    let view: MemoryView<u8> = wasm_memory.view();
    let topic_len = topic.len();
    let payload_len = payload.len();
    let topic_ptr = PTR_BUFFER_MESSAGE_TOPIC.load(Ordering::Relaxed) as usize;
    let payload_ptr = PTR_BUFFER_MESSAGE_PAYLOAD.load(Ordering::Relaxed) as usize;

    for (byte, cell) in topic.bytes()
        .zip(view[topic_ptr..(topic_ptr + topic_len)].iter())
    {
        cell.set(byte);
    }

    for (byte, cell) in payload.bytes()
        .zip(view[payload_ptr..(payload_ptr + payload_len)].iter())
    {
        cell.set(byte);
    }

    (topic_len as i32, payload_len as i32)
}

// Commonly used accross thread functions that are exported by the EEA WASM module
pub fn get_exported_eea_apis(wasm_instance: Instance) -> (
    NativeFunc<i64, i32>, NativeFunc<(), i32>, NativeFunc<i32, i32>, NativeFunc<(i32, i32), i32>, NativeFunc<(i32, i32), i32>
) {
    (
        wasm_instance.exports.get_native_function("eea_loop").unwrap(),
        wasm_instance.exports.get_native_function("eea_shutdown").unwrap(),
        wasm_instance.exports.get_native_function("eea_set_connection_status").unwrap(),
        wasm_instance.exports.get_native_function("eea_message_received").unwrap(),
        wasm_instance.exports.get_native_function("eea_direct_trigger").unwrap(),
    )
}

// EEA direct trigger helper
pub fn send_direct_trigger(
    wasm_memory: &Memory, direct_id: String, payload: String, eea_direct_trigger: NativeFunc<(i32, i32), i32>
) {
    let (direct_id_len, payload_len) = set_message_buffer(wasm_memory, payload, direct_id);
    let result = eea_direct_trigger.call(direct_id_len, payload_len).unwrap();

    if result != 0 { eprintln!("EEA Error-code: {:?}", result); }

    println!("(info, direct, exit) >");
}

// Loads and configures a WASM module from a byte array.
pub fn load_wasm_bundle(mqtt_client: Client) -> Result<WasmInfo, Box<dyn Error>> {
    // WASM compiler, or runtime here
    let wasm_compiler = Cranelift::new();
    let wasm_store = Store::new(&Universal::new(wasm_compiler).engine());

    // for imported memory, exported memory is not supported in this example
    let mem_type = MemoryType::new(5, None, false);
    let wasm_memory = Memory::new(&wasm_store, mem_type)?;

    let wasm_exists = Path::new(&CONFIGS.eea_bundle_path).exists();

    if !wasm_exists {
        println!("No EEA WASM bundle file detected at configured location.");

        // create a temporary empty instance
        let module = Module::new(&wasm_store, "(module)")?;
        let imports = imports!{};
        let instance = Instance::new(&module, &imports)?;
        return Ok(WasmInfo { // defaults for no bundle
            instance,
            memory: wasm_memory,
            bundle_id: DEFAULT_BUNDLE_ID.to_owned(),
        });
    }

    // read from WASM file, if gzipped (eea_bundle_gzip set within resources/eea_config.toml) attempt to decode
    let wasm_bytes = read(&CONFIGS.eea_bundle_path)?;
    let module: Module = if CONFIGS.eea_bundle_gzip {
        let mut decoder = Decoder::new(&wasm_bytes[..])?;
        let mut decoded_data = Vec::new();
        decoder.read_to_end(&mut decoded_data)?;
        Module::new(&wasm_store, decoded_data)?
    } else {
        Module::new(&wasm_store, wasm_bytes)?
    };

    let env = Env {
        memory: wasm_memory.clone(),
        mqtt_client
    };

    // functions that are defined in native code and imported into the EEA WASM module
    // user created registered functions also go here
    let imports = imports!{
        "env" => {
            "memory" => wasm_memory.clone(),
            "eea_trace" => Function::new_native_with_env(&wasm_store, env.clone(), eea_api::eea_trace),
            "eea_send_message" => Function::new_native_with_env(&wasm_store, env.clone(), eea_api::eea_send_message),
            "eea_get_time" => Function::new_native_with_env(&wasm_store, env.clone(), eea_api::eea_get_time),
            "eea_get_device_id" => Function::new_native_with_env(&wasm_store, env.clone(), eea_api::eea_get_device_id),
            "eea_storage_save" => Function::new_native_with_env(&wasm_store, env.clone(), eea_api::eea_storage_save),
            "eea_storage_read" => Function::new_native_with_env(&wasm_store, env.clone(), eea_api::eea_storage_read),
            "eea_set_message_buffers" => Function::new_native(&wasm_store, eea_api::eea_set_message_buffers),
            "eea_sleep" => Function::new_native(&wasm_store, eea_api::eea_sleep),
            // add registered_functions
            "eea_fn_terminal_print" => Function::new_native_with_env(&wasm_store, env, registered_functions::eea_fn_terminal_print),
        }
    };

    println!("Initializing WASM...");
    let wasm_instance = Instance::new(&module, &imports)?;

    configure_eea_wasm(wasm_instance.clone())?;

    // initialize WASM
    let eea_init: NativeFunc<(), i32> = wasm_instance.exports.get_native_function("eea_init")?;
    let init_res = eea_init.call()?;
    if init_res != 0 {
        eprintln!("WASM initialization failed, EEA Error-code: {:?}", init_res);
        exit(1);
    }

    // bundle id pointer globals
    let bundle_id_str_ptr = wasm_instance.exports.get_global("BUNDLE_IDENTIFIER")?.get().unwrap_i32();
    let bundle_id_length_ptr = wasm_instance.exports.get_global("BUNDLE_IDENTIFIER_LENGTH")?.get().unwrap_i32();

    let bundle_id = get_bundle_id(wasm_memory.clone(), bundle_id_str_ptr, bundle_id_length_ptr);

    println!("WASM Running.");

    Ok(WasmInfo {
        instance: wasm_instance,
        memory: wasm_memory,
        bundle_id,
    })
}
