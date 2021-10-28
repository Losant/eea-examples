#ifndef EEA_API_H
#define EEA_API_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <wasm3.h>
#include <m3_env.h>

class EEA_Runtime;

class EEA_API {
  public:
    EEA_API(EEA_Runtime *eea_runtime, IM3Module wasm_module, QueueHandle_t xQueueMQTT);
    QueueHandle_t xQueueMQTT;
    EEA_Runtime *eea_runtime;
};

#endif