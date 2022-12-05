/*
    EEA config/header file

    contains globals and structs
*/
use miniserde::Deserialize;
use toml;
use rumqttc::Client;
use wasmer::{Memory, Instance, WasmerEnv};
use once_cell::sync::Lazy;
use std::process::exit;
use std::fs::read_to_string;
use std::sync::atomic::{AtomicI32, AtomicBool};

// set the absolute or relative path to your eea_config toml file
static CONFIG_PATH: &str = "resources/eea_config.toml";

// Buffer message config (atomics)
pub static PTR_BUFFER_MESSAGE_TOPIC: AtomicI32 = AtomicI32::new(0);
pub static BUFFER_MESSAGE_TOPIC_LENGTH: AtomicI32 = AtomicI32::new(0);
pub static PTR_BUFFER_MESSAGE_PAYLOAD: AtomicI32 = AtomicI32::new(0);
pub static BUFFER_MESSAGE_PAYLOAD_LENGTH: AtomicI32 = AtomicI32::new(0);

// what to print for each trace level type
pub static TRACE_LEVELS: &'static [&'static str] = &["", "ERROR", "INFO"];

// thread through current MQTT connection status
pub static MQTT_CONNECTED: AtomicBool = AtomicBool::new(false);

// Config struct holds configs from eea_config toml.
#[derive(Deserialize)]
pub struct Config {
    pub eea_broker: String,
    pub eea_device_id: String,
    pub eea_access_key: String,
    pub eea_access_secret: String,
    pub eea_base_topic: String,

    pub eea_trace_topic_level: i32,
    pub eea_version: String,
    pub eea_stack_size: i32,
    pub eea_export_memory: bool,
    pub eea_disable_debug_msg: bool,
    pub eea_debug_symbols: bool,
    pub eea_bundle_gzip: bool,
    pub eea_bundle_path: String,

    pub eea_storage_size: i32,
    pub eea_storage_interval: i32,
    pub eea_storage_path: String,

    pub eea_main_loop_interval: u64
}

// lazy load Config from file for use as static
pub static CONFIGS: Lazy<Config> = Lazy::new(|| {
    // TODO: add a method to accept environment variables
    let contents = match read_to_string(CONFIG_PATH) {
        Ok(c) => c,
        Err(_) => {
            eprintln!("Could not read file `{}`", CONFIG_PATH);
            exit(1);
        }
    };

    match toml::from_str(&contents) {
        Ok(d) => d,
        Err(_) => {
            eprintln!("Unable to load data from `{}`", CONFIG_PATH);
            exit(1);
      }
    }
});

// WASM Environment variables to share with imported EEA functions 
#[derive(WasmerEnv, Clone)]
pub struct Env {
    pub memory: Memory,
    pub mqtt_client: Client
}

// Currently running WASM info
pub struct WasmInfo {
    pub instance: Instance,
    pub memory: Memory,
    pub bundle_id: String,
}
