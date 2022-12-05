/*
    The EEA WASM API functions
        as outlined in the docs

    These functions are defined in native code and imported into the EEA WASM module
    Note: although WASM only really supports the data types i32, i64, f32, and f64
          we can declare many of them different in the args for casting 
*/
use crate::configs::{
    CONFIGS, Env, PTR_BUFFER_MESSAGE_TOPIC, PTR_BUFFER_MESSAGE_PAYLOAD,
    BUFFER_MESSAGE_TOPIC_LENGTH, BUFFER_MESSAGE_PAYLOAD_LENGTH, TRACE_LEVELS
};

use rumqttc::QoS;
use wasmer::{MemoryView, WasmPtr, Array};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use std::sync::atomic::Ordering;
use std::fs::{read, write};
use std::thread;

// print WASM info and error messages
pub fn eea_trace(env: &Env, message: WasmPtr<u8, Array>, len: u32, level: i32) -> i32 {
    // Use helper method on `WasmPtr` to read a utf8 string
    let string = message.get_utf8_string(&env.memory, len).unwrap();
    println!("{}: {}", TRACE_LEVELS[level as usize], string);

    0
}

// publish mqtt message, the driver will queue it
pub fn eea_send_message(
    env: &Env,
    topic: WasmPtr<u8, Array>,
    topic_len: u32,
    payload: WasmPtr<u8, Array>,
    payload_len: u32, qos: i32
) -> i32 {
    let topic = topic.get_utf8_string(&env.memory, topic_len).unwrap();
    let payload = payload.get_utf8_string(&env.memory, payload_len).unwrap();

    let qos_type = match qos {
        0 => QoS::AtMostOnce,
        1 => QoS::AtLeastOnce,
        _ => QoS::ExactlyOnce
    };

    env.mqtt_client.clone().publish(
        topic.clone(),
        qos_type,
        false,
        payload
    )
    .unwrap();

    println!("Sent {:?} message.", topic);

    0
}

// send current system time in milliseconds since epoch
pub fn eea_get_time(env: &Env, out_timestamp: i32) -> i32 {
    let view: MemoryView<u8> = env.memory.view();
    let since_the_epoch = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis();
    let epoch_bytes = since_the_epoch.to_le_bytes();

    for (i, byte) in epoch_bytes.iter().enumerate() {
        view[(out_timestamp as usize) + i].set(*byte)
    }

    0
}

/*
  Called by the EEA to provide pre-allocated message buffers.
  These buffers are used to send data to the EEA for 
  direct triggers, commands, etc.
*/
pub fn eea_set_message_buffers(topic: i32, topic_len: i32, payload: i32, payload_len: i32) -> i32 {
    PTR_BUFFER_MESSAGE_TOPIC.store(topic, Ordering::Relaxed);
    BUFFER_MESSAGE_TOPIC_LENGTH.store(topic_len, Ordering::Relaxed);
    PTR_BUFFER_MESSAGE_PAYLOAD.store(payload, Ordering::Relaxed);
    BUFFER_MESSAGE_PAYLOAD_LENGTH.store(payload_len, Ordering::Relaxed);

    0
}

// sleep/delay the WASM thread by given milliseconds
pub fn eea_sleep(millis: i32) -> i32 {
    thread::sleep(Duration::from_millis(millis as u64));

    0
}

// save current WASM storage to configured file
pub fn eea_storage_save(env: &Env, store_values: WasmPtr<u8, Array>, len: u32) -> i32 {
    write(&CONFIGS.eea_storage_path,
        store_values.get_utf8_string(&env.memory, len).unwrap()
    ).expect("Unable to write file!");

    0
}

// read configured storage file into WASM memory
pub fn eea_storage_read(env: &Env, out_id: i32, buff_len: i32, out_id_len: i32) -> i32 {
    let view: MemoryView<u8> = env.memory.view();
    let storage_values = read(&CONFIGS.eea_storage_path).expect("Buffer overflow!");
    let storage_len = storage_values.len();

    if storage_len > buff_len as usize {
        eprintln!("Buffer not large enough to encode string. Buffer capacity: {:?}.", buff_len);
        return 1
    }

    for (byte, cell) in storage_values.into_iter()
        .zip(view[out_id as usize..(out_id as usize + storage_len) as usize].iter())
    {
        cell.set(byte);
    }

    view[out_id_len as usize].set(storage_len as u8);

    0
}

// retrieved configured device id
pub fn eea_get_device_id(env: &Env, out_id: i32, buff_len: i32, out_id_len: i32) -> i32 {
    let view: MemoryView<u8> = env.memory.view();

    for (byte, cell) in CONFIGS.eea_device_id.bytes()
        .zip(view[out_id as usize..(out_id + buff_len) as usize].iter())
    {
        cell.set(byte);
    }

    view[out_id_len as usize].set(buff_len as u8);

    0
}
