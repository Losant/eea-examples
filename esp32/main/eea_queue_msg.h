#ifndef EEA_QUEUE_MSG_H
#define EEA_QUEUE_MSG_H

#include "eea_config.h"

/**
 * This contains normal MQTT messages
 * to/from the EEA.
 */
struct EEA_Queue_Msg
{
  char topic[EEA_TOPIC_SIZE_BYTES];
  char payload[EEA_PAYLOAD_SIZE_BYTES];

  uint16_t topic_length;
  uint32_t payload_length;
  uint8_t qos;
};

/**
 * This contains compiled wasm bundles.
 */
struct EEA_Queue_Msg_Flow
{
  char bundle[EEA_MAX_WASM_BUNDLE_SIZE];
  uint32_t bundle_size;
};

#endif
