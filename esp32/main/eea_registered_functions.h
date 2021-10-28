#ifndef EEA_REGISTERED_FUNCTIONS_H
#define EEA_REGISTERED_FUNCTIONS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <wasm3.h>
#include <m3_env.h>

class EEA_Registered_Functions {
  public:
    EEA_Registered_Functions(IM3Module wasm_module);
};

#endif