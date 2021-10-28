#ifndef EEA_CONFIG_H
#define EEA_CONFIG_H

// Losant device credentials.
#define LOSANT_DEVICE_ID "YOUR_DEVICE_ID"
#define LOSANT_ACCESS_KEY "YOUR_ACCESS_KEY"
#define LOSANT_ACCESS_SECRET "YOUR_ACCESS_SECRET"

// Message buffer sizes.
#define EEA_TOPIC_SIZE_BYTES 256
#define EEA_PAYLOAD_SIZE_BYTES 1024

// Losant MQTT broker configuration.
#define EEA_BROKER_URL "mqtt://broker.losant.com"
#define EEA_BROKER_PORT 1883

// The maximum wasm bundle size is 256kb.
// Most bundles are a little over 100kb.
#define EEA_MAX_WASM_BUNDLE_SIZE 262144

#endif