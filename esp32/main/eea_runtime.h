#ifndef EEA_RUNTIME_H
#define EEA_RUNTIME_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "eea_api.h"
#include "eea_registered_functions.h"
#include "eea_queue_msg.h"

#include <wasm3.h>
#include <m3_env.h>

class EEA_Runtime {
  public:
    EEA_Runtime(QueueHandle_t xQueueMQTT, QueueHandle_t xQueueEEA, QueueHandle_t xQueueFlows);
    QueueHandle_t xQueueEEA;
    QueueHandle_t xQueueMQTT;
    QueueHandle_t xQueueFlows;
    QueueHandle_t xQueueNVS;
    EEA_API *eea_api;
    EEA_Registered_Functions *eea_registered_functions;

    IM3Function eea_loop;
    IM3Function eea_message_received;
    IM3Function eea_set_connection_status;
    IM3Environment wasm_env;
    IM3Runtime wasm_runtime;
    IM3Module wasm_module;

    EEA_Queue_Msg_Flow *bundle = NULL;

    char *message_buffer_topic;
    uint16_t message_buffer_topic_length;
    char *message_buffer_payload;
    uint32_t message_buffer_payload_length;

    char *bundle_id;
    bool connected = false;

  private:
    StaticTask_t xTaskBuffer;
    StaticQueue_t xStaticQueueNVS;
    StackType_t *xStack;
    TaskHandle_t xSaveBundleTaskHandle;
};

#endif