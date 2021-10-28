#include "esp_log.h"
#include "driver/gpio.h"

#include "eea_registered_functions.h"

#include <wasm3.h>
#include <m3_env.h>

static const char *TAG = "EEA_API";

m3ApiRawFunction(eea_fn_read_accelerometer)
{
    ESP_LOGI(TAG, "eea_fn_read_accelerometer");
    m3ApiReturnType(int32_t)

    const char *accel = "[0, 1.5, 2]";

    m3ApiGetArgMem(char*, readings_buffer);
    m3ApiGetArg(uint32_t, buffer_length);
    m3ApiGetArgMem(char*, bytes_written_buffer);

    uint32_t length = strlen(accel);
    strncpy(readings_buffer, accel, buffer_length);
    memcpy(bytes_written_buffer, &length, sizeof(length));

    ESP_LOG_BUFFER_CHAR(TAG, readings_buffer, strlen(accel));

    m3ApiReturn(0)
}

m3ApiRawFunction(eea_fn_gpio_set_direction)
{
  ESP_LOGI(TAG, "eea_fn_gpio_set_direction");
  m3ApiReturnType(int32_t)

  m3ApiGetArg(uint8_t, pin);
  m3ApiGetArg(uint32_t, mode);

  int32_t result = gpio_set_direction((gpio_num_t)pin, gpio_mode_t(mode));

  m3ApiReturn(result);
}

m3ApiRawFunction(eea_fn_gpio_set_level)
{
  ESP_LOGI(TAG, "eea_fn_gpio_set_level");
  m3ApiReturnType(int32_t)

  m3ApiGetArg(uint8_t, pin);
  m3ApiGetArg(uint32_t, level);

  int32_t result = gpio_set_level((gpio_num_t)pin, level);

  m3ApiReturn(result);
}

EEA_Registered_Functions::EEA_Registered_Functions(IM3Module wasm_module)
{

  // The imports are commented out since these are unlikely to apply to
  // every use case. The code is left here for reference.
  
  /*
  const char* module_name = "env";

  M3Result result = m3Err_none;

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_fn_gpio_set_direction", "i(ii)", &eea_fn_gpio_set_direction);
  if(result != m3Err_none) {
    ESP_LOGI(TAG, "eea_fn_gpio_set_direction link %s", result);
  }

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_fn_gpio_set_level", "i(ii)", &eea_fn_gpio_set_level);
  if(result != m3Err_none) {
    ESP_LOGI(TAG, "eea_fn_gpio_set_level link %s", result);
  }

  result = m3_LinkRawFunction(wasm_module, module_name, "eea_fn_read_accelerometer", "i(*i*)", &eea_fn_read_accelerometer);
  if(result != m3Err_none) {
    ESP_LOGI(TAG, "eea_fn_read_accelerometer link %s", result);
  }
  */
}