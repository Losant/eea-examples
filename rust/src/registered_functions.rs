/*
    EEA custom functions

    Add your registered functions here, you'll also need to add them to the import object
        within wasm_helpers.rs under add registered_functions.
*/
use crate::configs::Env;
use wasmer::{WasmPtr, Array};

// prints a Losant EEA Workflow message to your terminal screen
pub fn eea_fn_terminal_print(env: &Env, message: WasmPtr<u8, Array>, len: i32) -> i32 {
    // Use helper method on `WasmPtr` to read a utf8 string
    let string = message.get_utf8_string(&env.memory, len as u32).unwrap();
    println!("{}", string);

    0
}
