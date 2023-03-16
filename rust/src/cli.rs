/*
    A simple interactive CLI

    accepts the commands info, direct, and exit
    direct requires a direct id and JSON payload string
    e.g. "direct myId123 { "temp": 98.7 }"
*/
use crate::configs::{CONFIGS, MQTT_CONNECTED};

use std::io::Write;
use std::thread;
use std::time::Duration;
use std::sync::{Mutex, Arc};
use std::sync::atomic::Ordering;

// simple interactive prompt loop
pub fn cli_prompt(user_input_queue: Arc<Mutex<Vec<String>>>) {
    let mut prompted = false;
    loop {
        let mut line = String::new();
        if !prompted {
            println!("(info, direct, exit) >");
            prompted = true;
        }
        std::io::stdout().flush().unwrap();
        std::io::stdin().read_line(&mut line).expect("Could not read a line!");

        let input = line.trim().to_string();
        user_input_queue.lock().unwrap().push(input);

        thread::sleep(Duration::from_millis(100));
    }
}

// example of currently available application info
pub fn display_info(bundle_id: &str) {
    println!(r#"    EEA Version: {}

    Device ID: {}
    MQTT Broker: {}
    MQTT Base Topic: {}
    Connected: {}

    WASM Bundle ID: {}
    WASM Loop Interval (ms): {}
    WASM Trace Level: {}
    WASM Stack Size (bytes): {}
    WASM Path: {}           
        "#,
        CONFIGS.eea_version, CONFIGS.eea_device_id, CONFIGS.eea_broker, CONFIGS.eea_base_topic,
        MQTT_CONNECTED.load(Ordering::Relaxed), bundle_id, CONFIGS.eea_main_loop_interval,
        CONFIGS.eea_trace_topic_level, CONFIGS.eea_stack_size, CONFIGS.eea_bundle_path
    );
    
    println!("(info, direct, exit) >");
}
