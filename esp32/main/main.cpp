/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "protocol_examples_common.h"

#include "eea_queue_msg.h"
#include "eea_runtime.h"
#include "eea_mqtt.h"

#define GPIO_OUTPUT_IO_RED 32
#define GPIO_OUTPUT_IO_GREEN 12
#define GPIO_OUTPUT_IO_BLUE 14

static const char *TAG = "EEA_MAIN";

extern "C" void app_main(void)
{
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Connect to WiFi.
  // The simple example_connect() function does not handle timeouts, does not
  // gracefully handle various error conditions, and is only suited for use in examples.
  // When developing real applications, this helper function needs to be replaced with
  // full Wi-Fi / Ethernet connection handling code.
  // Details: https://github.com/espressif/esp-idf/tree/master/examples/protocols
  ESP_ERROR_CHECK(example_connect());

  // Create the queues so the MQTT task can communicate with the EEA task.
  // Allocating queue memory from SPIRAM. The normal MQTT message queues
  // will hold 10 messages. The flows queue will hold 1 (since those are very large).
  ESP_LOGI(TAG, "Creating FreeRTOS queues.");
  StaticQueue_t xStaticQueueMQTT;
  StaticQueue_t xStaticQueueEEA;
  StaticQueue_t xStaticQueueFlows;

  uint8_t *mqtt_queue_buffer = (uint8_t*)heap_caps_malloc(2 * sizeof(EEA_Queue_Msg), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  uint8_t *eea_queue_buffer = (uint8_t*)heap_caps_malloc(2 * sizeof(EEA_Queue_Msg), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  uint8_t *flows_queue_buffer = (uint8_t*)heap_caps_malloc(1 * sizeof(EEA_Queue_Msg_Flow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  QueueHandle_t xQueueMQTT;
  QueueHandle_t xQueueEEA;
  QueueHandle_t xQueueFlows;

  xQueueMQTT = xQueueCreateStatic(10, sizeof(EEA_Queue_Msg), mqtt_queue_buffer, &xStaticQueueMQTT);
  xQueueEEA = xQueueCreateStatic(10, sizeof(EEA_Queue_Msg), eea_queue_buffer, &xStaticQueueEEA);
  xQueueFlows = xQueueCreateStatic(1, sizeof(EEA_Queue_Msg_Flow), flows_queue_buffer, &xStaticQueueFlows);

  if( xQueueMQTT == NULL || xQueueEEA == NULL || xQueueFlows == NULL)
  {
    ESP_LOGI(TAG, "Failed to create queues.");
  }

  ESP_LOGI(TAG, "Initializing EEA Runtime.");
  EEA_Runtime eea_runtime(xQueueMQTT, xQueueEEA, xQueueFlows);

  ESP_LOGI(TAG, "Initializing EEA MQTT.");
  EEA_MQTT eea_mqtt(xQueueMQTT, xQueueEEA, xQueueFlows);

  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  while(true) {
    vTaskDelay(xDelay);
  }
}
