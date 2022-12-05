/*
    EEA Rust main function

    Creates and monitors threads (MQTT, WASM, and CLI).
    Main loop takes action on queued messages.
*/
use crate::configs::{CONFIGS, MQTT_CONNECTED};
use crate::cli::{cli_prompt, display_info};
use crate::mqtt::{mqtt_handle_messaging, mqtt_config, mqtt_init, send_hello};
use crate::wasm_helpers::{
    set_message_buffer, get_exported_eea_apis, load_wasm_bundle, send_direct_trigger
};
mod configs;
mod cli;
mod mqtt;
mod wasm_helpers;
mod eea_api;
mod registered_functions;

use rumqttc::Client;
use std::sync::{Mutex, Arc};
use std::sync::atomic::Ordering;
use std::thread;
use std::error::Error;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use std::process::exit;

fn main() -> Result<(), Box<dyn Error>> {
    println!("EEA App Initializing...");

    let user_input_queue: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(Vec::new()));
    let user_input_queue_clone = user_input_queue.clone();
    let recieved_msg_queue: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(Vec::new()));
    let recieved_msg_queue_clone = recieved_msg_queue.clone();

    let mqtt_options = mqtt_config()?;
    let (mqtt_client, connection) = Client::new(mqtt_options, 10);
    let mqtt_client_clone = mqtt_client.clone();
    let mut mqtt_client_again = mqtt_client.clone();
    
    let mut wasm_info = load_wasm_bundle(mqtt_client)?;
    let bundle_id = wasm_info.bundle_id.clone();
    let (
        mut eea_loop, mut eea_shutdown, mut eea_set_connection_status,
        mut eea_message_received, mut eea_direct_trigger
    ) = get_exported_eea_apis(wasm_info.instance);

    // MQTT Threads:
    thread::spawn(move || {
        thread::spawn(move || {
            mqtt_init(mqtt_client_clone, &bundle_id);
            thread::sleep(Duration::from_millis(100));
        });

        mqtt_handle_messaging(connection, recieved_msg_queue);
    });

    // CLI Prompt Thread:
    thread::spawn(move || { cli_prompt(user_input_queue); });

    // main loop
    loop {
        // user input queue
        let mut user_inputs = user_input_queue_clone.lock().unwrap();
        for mut input in user_inputs.drain(..) {
            if input == "exit" { // shutdown WASM, MQTT, and exit
                eea_shutdown.call()?;
                mqtt_client_again.disconnect()?;
                exit(0);
            } else if input == "info" { // Current Info
                display_info(&wasm_info.bundle_id);
            } else if input.starts_with("direct") { // fire a direct trigger 
                input = input.replacen("direct ", "", 1);
                match input.split_once(" ") {
                    Some((direct_id, payload)) => {
                        send_direct_trigger(&wasm_info.memory, payload.to_owned(), direct_id.to_owned(), eea_direct_trigger.clone());
                    }
                    None => {
                        eprintln!("Invalid direct command format, should be: direct direct_id JSON_payload_string");
                        println!("(info, direct, exit) >");
                    }
                }
            }
        }
        drop(user_inputs); // otherwise we lockup other threads during sleep

        // mqtt message queue
        let mut mqtt_msgs = recieved_msg_queue_clone.lock().unwrap();
        for mut msg in mqtt_msgs.drain(..) {
            let msg_type = msg.remove(0);
            let msg_res = match msg_type {
                'u' => { // updating WASM
                    println!("Updating WASM...");
                    let mqtt_client_clone = mqtt_client_again.clone();
                    let mqtt_client_again = mqtt_client_again.clone();

                    eea_shutdown.call()?;

                    wasm_info = load_wasm_bundle(mqtt_client_clone)?; // the move and setup here is the issue
                    (eea_loop, eea_shutdown, eea_set_connection_status, eea_message_received, eea_direct_trigger) =
                        get_exported_eea_apis(wasm_info.instance);

                    send_hello(mqtt_client_again, &wasm_info.bundle_id);

                    0
                },
                'c' => { // connected
                    MQTT_CONNECTED.store(true, Ordering::Relaxed);
                    eea_set_connection_status.call(1)?
                },
                'd' => { // disconnected
                    MQTT_CONNECTED.store(false, Ordering::Relaxed);
                    eea_set_connection_status.call(0)?
                },
                'm' => { // command message
                    let topic = CONFIGS.eea_base_topic.clone() + "/" + &CONFIGS.eea_device_id + "/command";
                    let (topic_len, payload_len) = set_message_buffer(&wasm_info.memory, msg, topic);
                    eea_message_received.call(topic_len, payload_len)?
                },
                'v' => { // virtual button message
                    let topic = CONFIGS.eea_base_topic.clone() + "/" + &CONFIGS.eea_device_id + "/toAgent/virtualButton";
                    let (topic_len, payload_len) = set_message_buffer(&wasm_info.memory, msg, topic);
                    eea_message_received.call(topic_len, payload_len)?
                },
                _ => { 1 }// empty, err but keep going

            };
            if msg_res != 0 {
                eprintln!("Incoming message EEA Error-code: {:?}", msg_res);
            }
        }
        drop(mqtt_msgs); // otherwise we lockup other threads during sleep

        // run the WASM with current epoch time in milliseconds
        let since_the_epoch = SystemTime::now().duration_since(UNIX_EPOCH)?.as_millis();
        let result = eea_loop.call(since_the_epoch as i64)?;

        if result != 0 { eprintln!("EEA Error-code: {:?}", result); }

        // pause between main loop
        thread::sleep(Duration::from_millis(CONFIGS.eea_main_loop_interval));
    }
}
