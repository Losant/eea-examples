#ifndef EEA_MQTT_H
#define EEA_MQTT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

class EEA_MQTT {
  public:
    EEA_MQTT(QueueHandle_t xQueueMQTT, QueueHandle_t xQueueEEA, QueueHandle_t xQueueFlows);
    QueueHandle_t xQueueMQTT;
    QueueHandle_t xQueueEEA;
    QueueHandle_t xQueueFlows;
    bool is_connected;
  private:
    TaskHandle_t xHandle;
};

#endif