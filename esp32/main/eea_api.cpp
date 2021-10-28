/**
 * This file imports all required functions for the EEA API.
 */

#include "esp_log.h"

#include "eea_api.h"
#include "eea_config.h"
#include "eea_queue_msg.h"
#include "eea_runtime.h"

#include <wasm3.h>
#include <m3_env.h>

static const char *TAG = "EEA_API";

m3ApiRawFunction(eea_trace)
{
    ESP_LOGI(TAG, "eea_trace");

    m3ApiReturnType  (int32_t)

    m3ApiGetArgMem(const char*, buf);
    m3ApiGetArg(uint32_t, length);

    char log[1024];
    strncpy(log, buf, length);
    log[length] = '\0';

    ESP_LOGI(TAG, "%s", log);

    m3ApiReturn(0)
}

m3ApiRawFunction(eea_set_message_buffers)
{
    ESP_LOGI(TAG, "eea_set_message_buffers");
    m3ApiReturnType  (int32_t)

    m3ApiGetArgMem(char*, message_buffer_topic);
    m3ApiGetArg(uint16_t, message_buffer_topic_length);
    m3ApiGetArgMem(char*, message_buffer_payload);
    m3ApiGetArg(uint32_t, message_buffer_payload_length);

    EEA_API *eea_api = (EEA_API*)(_ctx->userdata);
    EEA_Runtime *eea_runtime = eea_api->eea_runtime;

    eea_runtime->message_buffer_topic = message_buffer_topic;
    eea_runtime->message_buffer_topic_length = message_buffer_topic_length;
    eea_runtime->message_buffer_payload = message_buffer_payload;
    eea_runtime->message_buffer_payload_length = message_buffer_payload_length;

    m3ApiReturn(0)
}

m3ApiRawFunction(eea_send_message)
{
    ESP_LOGI(TAG, "eea_send_message");
    m3ApiReturnType  (int32_t)

    EEA_API *eea_api = (EEA_API*)(_ctx->userdata);

    m3ApiGetArgMem(const char*, topic_buffer);
    m3ApiGetArg(uint16_t, topic_length);
    m3ApiGetArgMem(const char*, payload_buffer);
    m3ApiGetArg(uint32_t, payload_length);
    m3ApiGetArg(uint8_t, qos);

    char topic[EEA_TOPIC_SIZE_BYTES];
    strncpy(topic, topic_buffer, topic_length);
    topic[topic_length] = '\0';

    char payload[EEA_PAYLOAD_SIZE_BYTES];
    strncpy(payload, payload_buffer, payload_length);
    payload[payload_length] = '\0';

    ESP_LOGI(TAG, "%s", topic);
    ESP_LOGI(TAG, "%s", payload);

    EEA_Queue_Msg queue_msg;
    strcpy(queue_msg.topic, topic);
    strcpy(queue_msg.payload, payload);
    queue_msg.qos = qos;

    queue_msg.topic_length = topic_length;
    queue_msg.payload_length = payload_length;

    xQueueSend(eea_api->xQueueMQTT, &queue_msg, 0);

    m3ApiReturn(0)
}

m3ApiRawFunction(eea_storage_save)
{
    ESP_LOGI(TAG, "eea_storage_save");
    m3ApiReturnType  (int32_t)
    m3ApiReturn(0)
}

m3ApiRawFunction(eea_storage_read)
{
    ESP_LOGI(TAG, "eea_storage_read");
    m3ApiReturnType  (int32_t)
    m3ApiReturn(0)
}

m3ApiRawFunction(eea_sleep)
{
    ESP_LOGI(TAG, "eea_sleep");
    m3ApiReturnType  (int32_t)

    m3ApiGetArg(uint32_t, milliseconds);

    const TickType_t xDelay = milliseconds / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);

    m3ApiReturn(0)
}

m3ApiRawFunction(eea_get_device_id)
{
    ESP_LOGI(TAG, "eea_get_device_id");
    m3ApiReturnType  (int32_t)

    m3ApiGetArgMem(char*, device_id_buffer);
    m3ApiGetArg(uint8_t, buffer_length);
    m3ApiGetArgMem(char*, bytes_written_buffer);

    strncpy(device_id_buffer, LOSANT_DEVICE_ID, buffer_length);
    uint8_t device_id_length = strlen(LOSANT_DEVICE_ID);

    memcpy(bytes_written_buffer, &device_id_length, sizeof(device_id_length));

    m3ApiReturn(0)
}

m3ApiRawFunction(eea_get_time)
{
    ESP_LOGI(TAG, "eea_get_time");
    m3ApiReturnType  (int32_t)

    m3ApiGetArgMem(char*, time_buffer);

    // Since this is using time-since-boot when calling
    // eea_loop, this should output 0 to indicate
    // we do not have time-since-epoch.
    uint64_t time = 0;

    memcpy(time_buffer, &time, sizeof(time));

    m3ApiReturn(0)
}

EEA_API::EEA_API(EEA_Runtime *eea_runtime, IM3Module wasm_module, QueueHandle_t xQueueMQTT)
{
  this->xQueueMQTT = xQueueMQTT;
  this->eea_runtime = eea_runtime;

  const char* module_name = "env";

  M3Result result = m3Err_none;

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_trace", "i(*ii)", &eea_trace);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_trace link %s", result);
  }

  result = m3_LinkRawFunctionEx(wasm_module, module_name, "eea_set_message_buffers", "i(*i*i)", &eea_set_message_buffers, this);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_set_message_buffers link %s", result);
  }

  result = m3_LinkRawFunctionEx(wasm_module, module_name, "eea_send_message", "i(*i*ii)", &eea_send_message, this);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_send_message link %s", result);
  }

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_storage_save", "i(*i)", &eea_storage_save);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_storage_save link %s", result);
  }

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_storage_read", "i(*i*)", &eea_storage_read);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_storage_read link %s", result);
  }

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_sleep", "i(i)", &eea_sleep);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_sleep link %s", result);
  }   

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_get_device_id", "i(*i*)", &eea_get_device_id);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_get_device_id link %s", result);
  }

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_get_time", "i(*)", &eea_get_time);
  if(result != m3Err_none) {
      ESP_LOGI(TAG, "eea_get_time link %s", result);
  }
}