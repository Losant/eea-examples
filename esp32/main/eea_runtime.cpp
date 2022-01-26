#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "eea_runtime.h"
#include "eea_api.h"
#include "eea_registered_functions.h"
#include "eea_queue_msg.h"

#include <wasm3.h>
#include <m3_env.h>

#define WASM_STACK_SLOTS    (128 * 1024)
#define WASM_TASK_STACK     (768 * 1024)
#define EEA_RUNTIME_TASK_PRIORITY 4

#define EEA_RUNTIME_SAVE_BUNDLE_TASK_SIZE 4096
#define EEA_RUNTIME_SAVE_BUNDLE_TASK_PRIORITY 4

// Namespace and key when storing wasm bundles to NVS.
#define EEA_NVS_NAMESPACE "EEA"
#define EEA_NVS_KEY "eea_bundle"

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

  char topic[256];
  char payload[1024];
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

  ESP_LOGI(TAG, "Topic: %s", topic);
  ESP_LOGI(TAG, "Payload: %s", payload);

  EEA_Queue_Msg *msg = (EEA_Queue_Msg*)malloc(sizeof(EEA_Queue_Msg));
  strcpy(msg->topic, topic);
  strcpy(msg->payload, payload);
  msg->topic_length = topic_length;
  msg->payload_length = payload_length;
  msg->qos = 0;

  xQueueSend(xQueueMQTT, msg, 0);

  free(msg);
}

/**
 * Checks NVS for a persisted wasm bundle.
 * If exists, will queue bundle in xQueueFlows.
 * Logged error codes can be found in nvs.h.
 * 
 * Returns:
 *  0 if bundle exists and successfully loaded.
 *  1 if no bundle was loaded.
 */
int load_from_nvs(EEA_Runtime *eea_runtime)
{
  ESP_LOGI(TAG, "Attempting to load wasm bundle from NVS...");

  nvs_handle_t eea_nvs_handle;
  esp_err_t err;

  err = nvs_open(EEA_NVS_NAMESPACE, NVS_READONLY, &eea_nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "Failed to open NVS storage. Error: 0x%04x", err);
    return 1;
  }

  size_t required_size = 0;
  err = nvs_get_blob(eea_nvs_handle, EEA_NVS_KEY, NULL, &required_size);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "No bundle in storage or failed to read bundle size. Error: 0x%04x", err);
    nvs_close(eea_nvs_handle);
    return 1;
  }

  if(required_size == 0) {
    ESP_LOGI(TAG, "Bundle found in NVS, but size was 0.");
    return 1;
  }

  // There is a bundle in NVS. Allocate message, read, and add to queue.
  EEA_Queue_Msg_Flow *msg = (EEA_Queue_Msg_Flow*)heap_caps_malloc(1 * sizeof(EEA_Queue_Msg_Flow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  err = nvs_get_blob(eea_nvs_handle, EEA_NVS_KEY, msg->bundle, &required_size);
  if(err != ESP_OK) {
    ESP_LOGI(TAG, "Failed to read bundle from NVS. Error: 0x%04x", err);
    nvs_close(eea_nvs_handle);
    free(msg);
    return 1;
  }

  ESP_LOGI(TAG, "Bundle loaded from NVS. Size: %d", required_size);
  msg->bundle_size = required_size;
  xQueueSend(eea_runtime->xQueueFlows, msg, 0);
  free(msg);

  nvs_close(eea_nvs_handle);
  return 0;
}

/**
 * Saves a wasm bundle to NVS storage.
 */
void save_to_nvs(EEA_Runtime *eea_runtime) {
  
  ESP_LOGI(TAG, "Attempting to save wasm bundle to NVS...");

  nvs_handle_t eea_nvs_handle;
  esp_err_t err;

  err = nvs_open(EEA_NVS_NAMESPACE, NVS_READWRITE, &eea_nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "Failed to open NVS storage. Error: 0x%04x", err);
    return;
  }

  err = nvs_set_blob(eea_nvs_handle, EEA_NVS_KEY, eea_runtime->bundle->bundle, eea_runtime->bundle->bundle_size);
  if(err != ESP_OK) {
    ESP_LOGI(TAG, "Failed to save bundle to NVS. Error: 0x%04x", err);
    nvs_close(eea_nvs_handle);
    return;
  }

  err = nvs_commit(eea_nvs_handle);
  if(err != ESP_OK) {
    ESP_LOGI(TAG, "Failed to commit NVS. Error: 0x%04x", err);
    nvs_close(eea_nvs_handle);
    return;
  }

  ESP_LOGI(TAG, "Successfully saved bundle to NVS.");
  nvs_close(eea_nvs_handle);
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
      ESP_LOGI(TAG, "%s", eea_runtime->wasm_runtime->error_message);
  }

  result = m3_FindFunction (&eea_runtime->eea_message_received, eea_runtime->wasm_runtime, "eea_message_received");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_message_received find %s", result);
      ESP_LOGI(TAG, "%s", eea_runtime->wasm_runtime->error_message);
  }

  result = m3_FindFunction (&eea_config_set_trace_level, eea_runtime->wasm_runtime, "eea_config_set_trace_level");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_config_set_trace_level find %s", result);
      ESP_LOGI(TAG, "%s", eea_runtime->wasm_runtime->error_message);
  }

  result = m3_FindFunction (&eea_config_set_storage_size, eea_runtime->wasm_runtime, "eea_config_set_storage_size");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_config_set_storage_size find %s", result);
      ESP_LOGI(TAG, "%s", eea_runtime->wasm_runtime->error_message);
  }

  result = m3_FindFunction (&eea_config_set_storage_interval, eea_runtime->wasm_runtime, "eea_config_set_storage_interval");
  if (result != m3Err_none) {
      ESP_LOGI(TAG, "eea_config_set_storage_interval find %s", result);
      ESP_LOGI(TAG, "%s", eea_runtime->wasm_runtime->error_message);
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

  uint32_t memory_size = 0;
  char *mem = (char*)m3_GetMemory(eea_runtime->wasm_runtime, &memory_size, 0);

  int8_t bundle_id_length = (int8_t)mem[bundle_id_length_ptr];

  char bundle_id[bundle_id_length + 1];
  memcpy(bundle_id, &(mem[bundle_id_ptr]), bundle_id_length);
  bundle_id[bundle_id_length] = '\0';

  eea_runtime->bundle_id = bundle_id;

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
  
  M3Result result = m3Err_none;

  const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
  while(true) {

    if(eea_runtime->bundle != NULL) {
      result = m3_CallV(eea_runtime->eea_loop, (uint64_t)(esp_timer_get_time() / 1000));
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

        // Queue the bundle for saving.
        xQueueSend(eea_runtime->xQueueNVS, eea_runtime->bundle_id, 0);
      }
    }

    // Check for messages to send to the EEA.
    if(uxQueueMessagesWaiting(eea_runtime->xQueueEEA) > 0) {
      EEA_Queue_Msg *msg = (EEA_Queue_Msg*)malloc(sizeof(EEA_Queue_Msg));
      if(xQueueReceive(eea_runtime->xQueueEEA, msg, 0) == pdPASS) {
        
        ESP_LOGI(TAG, "Processing message from EEA queue.");
        memcpy(eea_runtime->message_buffer_topic, msg->topic, msg->topic_length);
        memcpy(eea_runtime->message_buffer_payload, msg->payload, msg->payload_length);
        m3_CallV(eea_runtime->eea_message_received, msg->topic_length, msg->payload_length);
      }
      free(msg);
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

/**
 * Task that saves wasm bundles to NVS.
 * Due to limitation in ESP, NVS operations can't be done on tasks in SPIRAM.
 * This task is in main memory and receives messages via queue.
 * 
 * pvParameters = *EEA_Runtime
 */
void eea_save_bundle_task(void *pvParameters)
{
  EEA_Runtime *eea_runtime = (EEA_Runtime*)pvParameters;

  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

  while(true) {
    // Check to see if there is a new WASM bundle to load.
    if(uxQueueMessagesWaiting(eea_runtime->xQueueNVS) > 0) {

      // The queue message is the bundle ID.
      char bundle_id[128];
      if(xQueueReceive(eea_runtime->xQueueNVS, bundle_id, 0) == pdPASS) {
        save_to_nvs(eea_runtime);
      }
    }

    vTaskDelay(xDelay);
  }
}

EEA_Runtime::EEA_Runtime(QueueHandle_t xQueueMQTT, QueueHandle_t xQueueEEA, QueueHandle_t xQueueFlows)
{
  this->xQueueMQTT = xQueueMQTT;
  this->xQueueEEA= xQueueEEA;
  this->xQueueFlows = xQueueFlows;

  // Create a queue for persisting wasm bundles. Due to limitation in ESP, flash (nvs) operations
  // cannot be performed from tasks in SPIRAM. We need a task in main memory, which the
  // runtime task (eea_runtime_task) can communicate with via this queue.
  // This queue holds bundle ID strings.
  this->xQueueNVS = xQueueCreate(1, 128);

  // Create the wasm bundle persisting task.
  xTaskCreate(eea_save_bundle_task, "eea_runtime_save_bundle_task",
    EEA_RUNTIME_SAVE_BUNDLE_TASK_SIZE, this, EEA_RUNTIME_SAVE_BUNDLE_TASK_SIZE, &(this->xSaveBundleTaskHandle));

  // WASM bundles can be pretty big. Allocating a bunch of memory (~512kb)
  // from SPIRAM for the runtime task.
  this->xStack = (StackType_t*)heap_caps_malloc(WASM_TASK_STACK * sizeof(StackType_t),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  this->eea_api = NULL;
  this->eea_registered_functions = NULL;

  xTaskCreateStatic(eea_runtime_task, "eea_runtime_task", 
    WASM_TASK_STACK, this, EEA_RUNTIME_TASK_PRIORITY,
    xStack, &(this->xTaskBuffer));

  // Attempt to load a wasm bundle from NVS.
  // If no bundle was found, report "nullVersion" in the Hello Message.
  // If a bundle was found, the function queues bundle in xQueueFlows.
  // Since this function (EEA_Runtime) is called from the main task, 
  // flash (nvs) operations can be done here.
  if(load_from_nvs(this) != 0) {
    send_hello_message("nullVersion", this->xQueueMQTT);
  }
}