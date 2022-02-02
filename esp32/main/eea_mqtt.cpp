/**
 * Makes an MQTT connection to Losant's broker and handles queued message data.
 * 
 * For simplicity, this example code is using an unencrypted connection.
 * For SSL, please see ESP's example here:
 * https://github.com/espressif/esp-idf/tree/master/examples/protocols/mqtt/ssl
 * 
 * This code is based on the example code here:
 * https://github.com/espressif/esp-idf/tree/master/examples/protocols/mqtt/tcp
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"

#include "eea_mqtt.h"
#include "eea_config.h"
#include "eea_queue_msg.h"

#define EEA_MQTT_TASK_SIZE 4096
#define EEA_MQTT_TASK_PRIORITY 4

// The max payload size from the broker is 256KB.
#define EEA_MQTT_IN_BUFFER_SIZE (1024 * 256)
#define EEA_MQTT_OUT_BUFFER_SIZE (1024 * 32)

static const char *TAG = "EEA_MQTT";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * Queues a connect or disconnect message.
 * Queue message with topic "#connect" or "#disconnect" with no payload.
 * This is picked up by the runtime to change the connected status of the EEA.
 * Received topics cannot have '#' characters in them, so this will never conflict with real messages.
 */
static void queue_connect_message(bool connected, EEA_MQTT *eea_mqtt)
{
  const char *topic = connected ? "#connect" : "#disconnect";
  EEA_Queue_Msg *msg = (EEA_Queue_Msg*)heap_caps_malloc(1 * sizeof(EEA_Queue_Msg), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  msg->topic_length = strlen(topic);
  strcpy(msg->topic, topic);
  msg->payload_length = 0;
  xQueueSend(eea_mqtt->xQueueEEA, msg, 0);
  free(msg);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;

  EEA_MQTT *eea_mqtt = (EEA_MQTT*)event->user_context;

  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

      char topic[EEA_TOPIC_SIZE_BYTES];
      sprintf(topic, "losant/%s/toAgent/#", LOSANT_DEVICE_ID);
      msg_id = esp_mqtt_client_subscribe(client, topic, 0);
      ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

      sprintf(topic, "losant/%s/command", LOSANT_DEVICE_ID);
      msg_id = esp_mqtt_client_subscribe(client, topic, 0);
      ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

      eea_mqtt->is_connected = true;

      queue_connect_message(true, eea_mqtt);

      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      eea_mqtt->is_connected = false;
      queue_connect_message(false, eea_mqtt);
      break;
    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      ESP_LOGI(TAG, "MQTT_EVENT_DATA");

      // Topics are not null-terminated from the client.
      // Null terminate it for logging.
      char topic_terminated[256];
      strncpy(topic_terminated, event->topic, event->topic_len);
      topic_terminated[event->topic_len] = '\0';

      ESP_LOGI(TAG, "Topic: %s", topic_terminated);
      ESP_LOGI(TAG, "Payload length: %d", event->data_len);

      // If this is a new WASM bundle, put it in the 
      // flows queue. Otherwise it goes in the regular
      // message queue.
      if(strnstr(event->topic, "flows", event->topic_len) != NULL) {
        // WASM bundles are pretty big (~150kb). Need to allocate this using SPIRAM.
        EEA_Queue_Msg_Flow *msg = (EEA_Queue_Msg_Flow*)heap_caps_malloc(1 * sizeof(EEA_Queue_Msg_Flow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        memcpy(msg->bundle, event->data, event->data_len);
        msg->bundle_size = event->data_len;
        xQueueSend(eea_mqtt->xQueueFlows, msg, 0);
        free(msg);

      } else {
        EEA_Queue_Msg *msg = (EEA_Queue_Msg*)heap_caps_malloc(1 * sizeof(EEA_Queue_Msg), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        strncpy(msg->topic, event->topic, event->topic_len);
        strncpy(msg->payload, event->data, event->data_len);
        msg->topic_length = event->topic_len;
        msg->payload_length = event->data_len;
        xQueueSend(eea_mqtt->xQueueEEA, msg, 0);
        free(msg);
      }

      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
          log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
          log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
          log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
          ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

      }
      break;
    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
}

void eea_mqtt_task(void *pvParameters)
{
  EEA_MQTT *eea_mqtt = (EEA_MQTT*)pvParameters;

  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = EEA_BROKER_URL,
    .port = EEA_BROKER_PORT,
    .client_id = LOSANT_DEVICE_ID,
    .username = LOSANT_ACCESS_KEY,
    .password = LOSANT_ACCESS_SECRET,
    .user_context = eea_mqtt,
    .buffer_size = EEA_MQTT_IN_BUFFER_SIZE,
    .out_buffer_size = EEA_MQTT_OUT_BUFFER_SIZE
  };

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
  esp_mqtt_client_start(client);

  ESP_LOGI(TAG, "MQTT client started.");

  const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
  while(true) {
    if(eea_mqtt->is_connected) {
      if(uxQueueMessagesWaiting(eea_mqtt->xQueueMQTT) > 0) {
        EEA_Queue_Msg *msg = (EEA_Queue_Msg*)heap_caps_malloc(1 * sizeof(EEA_Queue_Msg), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if(xQueueReceive(eea_mqtt->xQueueMQTT, msg, 0) == pdPASS) {
          ESP_LOGI(TAG, "Processing MQTT queue message.");
          ESP_LOGI(TAG, "Topic: %s", msg->topic);
          ESP_LOGI(TAG, "Payload: %s", msg->payload);
          esp_mqtt_client_publish(client, msg->topic, msg->payload, msg->payload_length, msg->qos, 0);
        }
        free(msg);
      }
    }
    vTaskDelay(xDelay);
  }
}

EEA_MQTT::EEA_MQTT(QueueHandle_t xQueueMQTT, QueueHandle_t xQueueEEA, QueueHandle_t xQueueFlows)
{
  this->xQueueMQTT = xQueueMQTT;
  this->xQueueEEA = xQueueEEA;
  this->xQueueFlows = xQueueFlows;
  this->is_connected = false;

  xTaskCreate(eea_mqtt_task, "eea_mqtt_task", EEA_MQTT_TASK_SIZE, this, EEA_MQTT_TASK_PRIORITY, &(this->xHandle));
}