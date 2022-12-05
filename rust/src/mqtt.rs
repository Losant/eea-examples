/*
    MQTT helper functions

    http://docs.losant.com/edge-compute/embedded-edge-agent/mqtt-specification/
*/
use crate::configs::CONFIGS;

use rustls::ClientConfig;
use rumqttc::{self, Client, MqttOptions, QoS, Transport, Event, Incoming, Connection};
use std::sync::{Mutex, Arc};
use std::fs::write;
use std::str::from_utf8;
use std::thread;
use std::error::Error;
use std::time::Duration;

// Checks MQTT messages and the connection status
// adds valid topics to the recieved_msg_queue
pub fn mqtt_handle_messaging(mut connection: Connection, recieved_msg_queue: Arc<Mutex<Vec<String>>>) {
    let update_topic = format!("{}/{}/toAgent/flows", CONFIGS.eea_base_topic.clone(),&CONFIGS.eea_device_id);
    let command_topic = format!("{}/{}/command", CONFIGS.eea_base_topic.clone(),&CONFIGS.eea_device_id);
    let virtual_topic = format!("{}/{}/toAgent/virtualButton", CONFIGS.eea_base_topic.clone(),&CONFIGS.eea_device_id);

    for (_i, notification) in connection.iter().enumerate() {
        let event = notification;

        if let Ok(Event::Incoming(Incoming::Publish(publish))) = event { // incoming topic publish messages
            if publish.topic == update_topic {
                write(&CONFIGS.eea_bundle_path, &publish.payload).expect("Unable to write file");
                recieved_msg_queue.lock().unwrap().push("u".to_owned());
            } else if publish.topic == command_topic {
                let payload = from_utf8(&publish.payload).unwrap().to_owned();
                recieved_msg_queue.lock().unwrap().push("m".to_owned() + &payload);
            } else if publish.topic == virtual_topic {
                let payload = from_utf8(&publish.payload).unwrap().to_owned();
                recieved_msg_queue.lock().unwrap().push("v".to_owned() + &payload);
            } // ignore the rest
        } else if let Ok(Event::Incoming(Incoming::ConnAck(connected))) = event { // connection status
            if connected.code == rumqttc::ConnectReturnCode::Success {
                recieved_msg_queue.lock().unwrap().push("c".to_owned());
            } else {
                eprintln!("MQTT Connection issue: {:?}", connected.code);
                recieved_msg_queue.lock().unwrap().push("d".to_owned());
            }
        } else if let Err(err) = event { // errs
            // err means disconnected, will automatically attempt to reconnect
            eprintln!("MQTT Error: {:?}", err);
            recieved_msg_queue.lock().unwrap().push("d".to_owned());
        }

        thread::sleep(Duration::from_millis(500));
    }
}

// MQTT client options
// TLS/SSL certificates are pulled from root
pub fn mqtt_config() -> Result<MqttOptions, Box<dyn Error>> {
    let mut mqtt_options = MqttOptions::new(&CONFIGS.eea_device_id, &CONFIGS.eea_broker, 8883);
    mqtt_options.set_keep_alive(Duration::from_secs(30));
    mqtt_options.set_credentials(&CONFIGS.eea_access_key, &CONFIGS.eea_access_secret);
    mqtt_options.set_max_packet_size(262144, 90508);

    // Use rustls-native-certs to load root certificates from the operating system.
    let mut root_cert_store = rustls::RootCertStore::empty();
    for cert in rustls_native_certs::load_native_certs().expect("Could not load platform certs!") {
        root_cert_store.add(&rustls::Certificate(cert.0))?;
    }

    let client_config = ClientConfig::builder()
        .with_safe_defaults()
        .with_root_certificates(root_cert_store)
        .with_no_client_auth();

    // Transport.tcp() for non-tls connection
    // https://docs.rs/rumqttc/latest/rumqttc/enum.Transport.html
    mqtt_options.set_transport(Transport::tls_with_config(client_config.into()));

    Ok(mqtt_options)
}

// MQTT after conntection topic subscriptions and messaging
pub fn mqtt_init(mut client: Client, bundle_id: &str) {
    client.subscribe(CONFIGS.eea_base_topic.clone() + "/" + &CONFIGS.eea_device_id + "/command", QoS::AtMostOnce)
        .unwrap();
    client.subscribe(CONFIGS.eea_base_topic.clone() + "/" + &CONFIGS.eea_device_id + "/toAgent/#", QoS::AtMostOnce)
        .unwrap();

    send_hello(client, bundle_id);
}

// hello message, status message required by Losant Platform
// https://docs.losant.com/edge-compute/embedded-edge-agent/mqtt-specification/#publishing-the-hello-message
pub fn send_hello(mut client: Client, bundle_id: &str) {
    let message = format!(r#"{{
        "service": "embeddedWorkflowAgent",
        "version": {:?}, 
        "bundle": "{bundle_id}",
        "compilerOptions": {{
            "exportMemory": {:?},
            "disableDebugMessage": {:?},
            "traceLevel": {:?},
            "debugSymbols": {:?},
            "stackSize": {:?},
            "gzip": {:?}
        }}
        }}"#,
        CONFIGS.eea_version, CONFIGS.eea_export_memory, CONFIGS.eea_disable_debug_msg,
        CONFIGS.eea_trace_topic_level, CONFIGS.eea_debug_symbols, CONFIGS.eea_stack_size,
        CONFIGS.eea_bundle_gzip
    );
    client.publish(
        CONFIGS.eea_base_topic.clone() + "/" + &CONFIGS.eea_device_id + "/fromAgent/hello",
        QoS::AtLeastOnce,
        false,
        message
    ).unwrap();
}
