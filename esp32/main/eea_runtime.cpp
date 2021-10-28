#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "eea_runtime.h"
#include "eea_api.h"
#include "eea_registered_functions.h"
#include "eea_queue_msg.h"

#include <wasm3.h>
#include <m3_env.h>

#define WASM_STACK_SLOTS    (128 * 1024)
#define WASM_TASK_STACK     (512 * 1024)
#define EEA_RUNTIME_TASK_PRIORITY 4

static const char *TAG = "EEA_RUNTIME";

/**
 * Sends the Hello message to the broker to communicate
 * which bundle is running and the specific compile flags
 * for this device.
 * bundle_version: "nullVersion" if no bundle loaded
 *   otherwise the value from the BUNDLE_IDENTIFIER WASM global.
 * 
 * http://docs.losant.com/edge-compute/embedded-edge-agent/agent-api/#bundle-identifier
 */
void send_hello_message(const char *bundle_version, QueueHandle_t xQueueMQTT)
{
  ESP_LOGI(TAG, "Sending hello message: %s", bundle_version);

  char topic[EEA_TOPIC_SIZE_BYTES];
  char payload[EEA_PAYLOAD_SIZE_BYTES];
  uint32_t topic_length;
  uint32_t payload_length;

  topic_length = sprintf(topic, "losant/%s/fromAgent/hello", LOSANT_DEVICE_ID);
  payload_length = sprintf(payload,  
  "{"
    "\"service\": \"embeddedWorkflowAgent\","
    "\"version\": \"1.0.0\","
    "\"bundle\": \"%s\","
    "\"compilerOptions\": {"
      "\"exportMemory\": true,"
      "\"traceLevel\": 2"
    "}"
  "}", bundle_version);

  ESP_LOGI(TAG, "Payload: %s", payload);

  EEA_Queue_Msg msg;
  strcpy(msg.topic, topic);
  strcpy(msg.payload, payload);
  msg.topic_length = topic_length;
  msg.payload_length = payload_length;
  msg.qos = 0;

  xQueueSend(xQueueMQTT, &msg, 0);
}

/**
 * Loads the WASM bundle from the provided buffer.
 */ 
void load_wasm(EEA_Runtime *eea_runtime, char *bundle, uint32_t bundle_size)
{
  M3Result result = m3Err_none;

  eea_runtime->wasm_env = m3_NewEnvironment();
  if(!eea_runtime->wasm_env) ESP_LOGI(TAG, "Error: NewEnvironment");

  eea_runtime->wasm_runtime = m3_NewRuntime (eea_runtime->wasm_env, WASM_STACK_SLOTS, NULL);
  if (!eea_runtime->wasm_runtime) ESP_LOGI(TAG, "Error: NewRuntime");

  result = m3_ParseModule(eea_runtime->wasm_env, &(eea_runtime->wasm_module), (unsigned char*)bundle, bundle_size);
  if (result) ESP_LOGI(TAG, "Error: ParseModule");

  result = m3_LoadModule(eea_runtime->wasm_runtime, eea_runtime->wasm_module);
  if (result) ESP_LOGI(TAG, "%s", result);

  ESP_LOGI(TAG, "Linking EEA API functions...");

  eea_runtime->eea_api = new EEA_API(eea_runtime, eea_runtime->wasm_module, eea_runtime->xQueueMQTT);
  eea_runtime->eea_registered_functions = new EEA_Registered_Functions(eea_runtime->wasm_module);

  IM3Function eea_init;
  IM3Function eea_config_set_trace_level;
  IM3Function eea_config_set_storage_size;
  IM3Function eea_config_set_storage_interval;

  result = m3_FindFunction (&eea_init, eea_runtime->wasm_runtime, "eea_init");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_init find %s", result);
  }

  result = m3_FindFunction (&eea_runtime->eea_loop, eea_runtime->wasm_runtime, "eea_loop");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_loop find %s", result);
  }

  result = m3_FindFunction (&eea_runtime->eea_message_received, eea_runtime->wasm_runtime, "eea_message_received");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_message_received find %s", result);
  }

  result = m3_FindFunction (&eea_config_set_trace_level, eea_runtime->wasm_runtime, "eea_config_set_trace_level");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_config_set_trace_level find %s", result);
  }

  result = m3_FindFunction (&eea_config_set_storage_size, eea_runtime->wasm_runtime, "eea_config_set_storage_size");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_config_set_storage_size find %s", result);
  }

  result = m3_FindFunction (&eea_config_set_storage_interval, eea_runtime->wasm_runtime, "eea_config_set_storage_interval");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_config_set_storage_interval find %s", result);
  }

  m3_CallV(eea_config_set_storage_size, 4096);
  m3_CallV(eea_config_set_storage_interval, 0);
  m3_CallV(eea_config_set_trace_level, 1);
  m3_CallV(eea_init);

  uint8_t eea_init_return_code = 0;
  m3_GetResultsV (eea_init, &eea_init_return_code);
  ESP_LOGI(TAG, "eea_init result %d", eea_init_return_code);

  // Extract the bundle ID and report a new Hello message.
  IM3Global g_bundle_id = m3_FindGlobal(eea_runtime->wasm_module, "BUNDLE_IDENTIFIER");
  IM3Global g_bundle_id_length = m3_FindGlobal(eea_runtime->wasm_module, "BUNDLE_IDENTIFIER_LENGTH");

  M3TaggedValue bundle_id_value;
  M3TaggedValue bundle_id_length_value;

  m3_GetGlobal(g_bundle_id, &bundle_id_value);
  m3_GetGlobal(g_bundle_id_length, &bundle_id_length_value);

  int32_t bundle_id_ptr = bundle_id_value.value.i32;
  int32_t bundle_id_length_ptr = bundle_id_length_value.value.i32;

  ESP_LOGI(TAG, "bundle_id_ptr: %d", bundle_id_ptr);
  ESP_LOGI(TAG, "bundle_id_length_ptr: %d", bundle_id_length_ptr);

  uint32_t memory_size = 0;
  char *mem = (char*)m3_GetMemory(eea_runtime->wasm_runtime, &memory_size, 0);

  int8_t bundle_id_length = (int8_t)mem[bundle_id_length_ptr];

  char bundle_id[bundle_id_length + 1];
  memcpy(bundle_id, &(mem[bundle_id_ptr]), bundle_id_length);
  bundle_id[bundle_id_length] = '\0';

  ESP_LOGI(TAG, "bundle_id_length: %d", bundle_id_length);
  ESP_LOGI(TAG, "bundle_id: %s", bundle_id);

  send_hello_message(bundle_id, eea_runtime->xQueueMQTT);
}

/**
 * Stops and de-allocates any currently running wasm bundle.
 */ 
void destroy_wasm(EEA_Runtime *eea_runtime)
{
  if(eea_runtime->bundle != NULL) {

    IM3Function eea_shutdown;
    m3_FindFunction (&eea_shutdown, eea_runtime->wasm_runtime, "eea_shutdown");
    m3_CallV(eea_shutdown);

    m3_FreeEnvironment(eea_runtime->wasm_env);
    m3_FreeRuntime(eea_runtime->wasm_runtime);

    delete eea_runtime->eea_api;
    delete eea_runtime->eea_registered_functions;

    free(eea_runtime->bundle);
    eea_runtime->bundle = NULL;
  }
}

/**
 * Main EEA Runtime task function.
 * pvParameters = *EEA_Runtime
 */ 
void eea_runtime_task(void *pvParameters)
{
  EEA_Runtime *eea_runtime = (EEA_Runtime*)pvParameters;

  // When the runtime task is initialized, immediately send the Hello Message.
  send_hello_message("nullVersion", eea_runtime->xQueueMQTT);

  M3Result result = m3Err_none;

  const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
  while(true) {

    if(eea_runtime->bundle != NULL) {
      int64_t start = esp_timer_get_time();
      result = m3_CallV(eea_runtime->eea_loop, (uint64_t)(esp_timer_get_time() / 1000));
      int64_t end = esp_timer_get_time();
      ESP_LOGI(TAG, "Loop time: %d", (int)((end - start) / 1000));
    }

    if(result != m3Err_none) {
      ESP_LOGI(TAG, "%s", result);
      break;
    }

    // Check to see if there is a new WASM bundle to load.
    if(uxQueueMessagesWaiting(eea_runtime->xQueueFlows) > 0) {
      // Destroy the previous wasm, if needed.
      destroy_wasm(eea_runtime);

      // Load up the new wasm.
      eea_runtime->bundle = (EEA_Queue_Msg_Flow*)heap_caps_malloc(1 * sizeof(EEA_Queue_Msg_Flow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if(xQueueReceive(eea_runtime->xQueueFlows, eea_runtime->bundle, 0) == pdPASS) {
        ESP_LOGI(TAG, "Processing new WASM bundle.");
        load_wasm(eea_runtime, eea_runtime->bundle->bundle, eea_runtime->bundle->bundle_size);
      }
    }

    // Check for messages to send to the EEA.
    if(uxQueueMessagesWaiting(eea_runtime->xQueueEEA) > 0) {
      EEA_Queue_Msg msg;
      if(xQueueReceive(eea_runtime->xQueueEEA, &msg, 0) == pdPASS) {
        
        ESP_LOGI(TAG, "Processing message from EEA queue.");
        memcpy(eea_runtime->message_buffer_topic, msg.topic, msg.topic_length);
        memcpy(eea_runtime->message_buffer_payload, msg.payload, msg.payload_length);
        m3_CallV(eea_runtime->eea_message_received, msg.topic_length, msg.payload_length);
      }
    }

    vTaskDelay(xDelay);
  }

  // This code gets hit if an eea_loop iteration fails.
  // Most commonly caused by an exception in the WASM.
  // To help with debugging, this code prints a stacktrace
  // and spins. In production, you may want to restart
  // the board or reload the WASM bundle.
  IM3BacktraceInfo info = m3_GetBacktrace(eea_runtime->wasm_runtime);

  if (info) {
    ESP_LOGI(TAG, "==== wasm backtrace:");

    int frameCount = 0;
    IM3BacktraceFrame curr = info->frames;
    while (curr)
    {
      ESP_LOGI(TAG, "\n  %d: 0x%06x - %s!%s",
        frameCount, curr->moduleOffset,
        m3_GetModuleName (m3_GetFunctionModule(curr->function)),
        m3_GetFunctionName (curr->function));
      curr = curr->next;
      frameCount++;
    }
  }

  while(true) {
    int32_t millis = 100;
    const TickType_t xDelay = millis / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);
  }
}

EEA_Runtime::EEA_Runtime(QueueHandle_t xQueueMQTT, QueueHandle_t xQueueEEA, QueueHandle_t xQueueFlows)
{
  this->xQueueMQTT = xQueueMQTT;
  this->xQueueEEA= xQueueEEA;
  this->xQueueFlows = xQueueFlows;

  // WASM bundles can be pretty big. Allocating a bunch of memory (~512kb)
  // from SPIRAM for the runtime task.
  this->xStack = (StackType_t*)heap_caps_malloc(WASM_TASK_STACK * sizeof(StackType_t),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  this->eea_api = NULL;
  this->eea_registered_functions = NULL;

  xTaskCreateStatic(eea_runtime_task, "eea_runtime_task", 
    WASM_TASK_STACK, this, EEA_RUNTIME_TASK_PRIORITY,
    xStack, &(this->xTaskBuffer));
}