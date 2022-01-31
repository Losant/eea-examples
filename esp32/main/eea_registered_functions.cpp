#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "eea_registered_functions.h"

#include <wasm3.h>
#include <m3_env.h>

static const char *TAG = "ESP32_GPIO";

/**
 * Wraps the ESP IDF gpio_set_direction function.
 * Used to configure GPIO pins as digital inputs or outputs.
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#_CPPv418gpio_set_direction10gpio_num_t11gpio_mode_t
 * Inputs:
 *  pin (Int32): the pin to set.
 *  mode (Int32): the pin mode.
 *    Docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#_CPPv411gpio_mode_t
 *    GPIO_MODE_DISABLE = 0
 *    GPIO_MODE_INPUT = (BIT1) = 1
 *    GPIO_MODE_OUTPUT = (BIT2) = 2
 * 
 * Returns the result of gpio_set_direction(). ESP_OK (0) for success.
 */
m3ApiRawFunction(eea_fn_gpio_set_direction)
{
  ESP_LOGI(TAG, "eea_fn_gpio_set_direction");

  m3ApiReturnType(int32_t)

  m3ApiGetArg(int32_t, pin);
  m3ApiGetArg(int32_t, mode);

  int32_t result = gpio_set_direction((gpio_num_t)pin, gpio_mode_t(mode));

  m3ApiReturn(result);
}

/**
 * Wraps the ESP IDF gpio_set_level function.
 * Used to control the value of digital outputs.
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#_CPPv414gpio_set_level10gpio_num_t8uint32_t
 * Inputs:
 *  pin (Int32): the pin to set.
 *  level (Int32): the level to set. For digital outputs, 0 or 1.
 * 
 * Returns the result of gpio_set_level(). ESP_OK (0) for success.
 */
m3ApiRawFunction(eea_fn_gpio_set_level)
{
  ESP_LOGI(TAG, "eea_fn_gpio_set_level");
  m3ApiReturnType(int32_t)

  m3ApiGetArg(int32_t, pin);
  m3ApiGetArg(int32_t, level);

  int32_t result = gpio_set_level((gpio_num_t)pin, level);

  m3ApiReturn(result);
}

/**
 * Wraps the ESP IDF gpio_get_level function.
 * Used to read the value of a digital input.
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#_CPPv414gpio_get_level10gpio_num_t
 * Inputs:
 *  pin (Int32): the pin to get.
 * 
 * Outputs:
 *  value (Int32): the GPIO value (0 or 1).
 * 
 * Always returns 0.
 */
m3ApiRawFunction(eea_fn_gpio_get_level)
{
  ESP_LOGI(TAG, "eea_fn_gpio_get_level");
  m3ApiReturnType(int32_t)

  m3ApiGetArg(int32_t, pin);
  m3ApiGetArgMem(int32_t*, value);

  int32_t gpio_value = gpio_get_level((gpio_num_t)pin);
  memcpy(value, &gpio_value, sizeof(gpio_value));

  m3ApiReturn(0);
}

/**
 * Wraps the ESP IDF adc1_config_channel_atten function.
 * Must be called prior to reading any ADC channel.
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv425adc2_config_channel_atten14adc2_channel_t11adc_atten_t
 * Inputs:
 *   channel (Int32): the ADC channel to configure.
 *   atten (Int32): the attenuation to configure.
 *     ADC_ATTEN_DB_0 = 0
 *     ADC_ATTEN_DB_2_5 = 1
 *     ADC_ATTEN_DB_6 = 2
 *     ADC_ATTEN_DB_11 = 3
 * 
 * Returns the result of gpio_set_level(). ESP_OK (0) for success.
 */
m3ApiRawFunction(eea_fn_adc1_config_channel_atten)
{
  ESP_LOGI(TAG, "eea_fn_adc1_config_channel_atten");
  m3ApiReturnType(int32_t)

  m3ApiGetArg(int32_t, channel);
  m3ApiGetArg(int32_t, atten);

  int32_t result = adc1_config_channel_atten((adc1_channel_t)channel, (adc_atten_t)atten);

  m3ApiReturn(result);
}

/**
 * Wraps the ESP IDF adc1_config_width function.
 * Must be called prior to reading from ADC1.
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv417adc1_config_width16adc_bits_width_t
 * Inputs:
 *   width (Int32): the capture width.
 *     ADC_WIDTH_BIT_9 = 0
 *     ADC_WIDTH_BIT_10 = 1
 *     ADC_WIDTH_BIT_11 = 2
 *     ADC_WIDTH_BIT_12 = 3
 * 
 * Returns the result of adc1_config_width(). ESP_OK (0) for success.
 */
m3ApiRawFunction(eea_fn_adc1_config_width)
{
  ESP_LOGI(TAG, "eea_fn_adc1_config_width");
  m3ApiReturnType(int32_t)

  m3ApiGetArg(int32_t, width);

  int32_t result = adc1_config_width((adc_bits_width_t)width);

  m3ApiReturn(result);
}

/**
 * Wraps the ESP IDF adc1_get_raw function.
 * Called to receive the raw ADC value.
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv412adc1_get_raw14adc1_channel_t
 * Inputs:
 *   channel (Int32): the channel to read.
 * 
 * Outputs:
 *   value (Int32): the value. -1 if there's an error.
 * 
 * Always returns 0.
 */
m3ApiRawFunction(eea_fn_adc1_get_raw)
{
  ESP_LOGI(TAG, "eea_fn_adc1_get_raw");
  m3ApiReturnType(int32_t)

  m3ApiGetArg(int32_t, channel);
  m3ApiGetArgMem(int32_t*, value);

  int32_t adc_value = adc1_get_raw((adc1_channel_t )channel);
  memcpy(value, &adc_value, sizeof(adc_value));

  m3ApiReturn(0);
}


EEA_Registered_Functions::EEA_Registered_Functions(IM3Module wasm_module)
{
  const char* module_name = "env";

  m3_LinkRawFunction(wasm_module, module_name, "eea_fn_gpio_set_direction", "i(ii)", &eea_fn_gpio_set_direction);
  m3_LinkRawFunction(wasm_module, module_name, "eea_fn_gpio_set_level", "i(ii)", &eea_fn_gpio_set_level);
  m3_LinkRawFunction(wasm_module, module_name, "eea_fn_gpio_get_level", "i(i*)", &eea_fn_gpio_get_level);
  m3_LinkRawFunction(wasm_module, module_name, "eea_fn_adc1_config_channel_atten", "i(ii)", &eea_fn_adc1_config_channel_atten);
  m3_LinkRawFunction(wasm_module, module_name, "eea_fn_adc1_config_width", "i(i)", &eea_fn_adc1_config_width);
  m3_LinkRawFunction(wasm_module, module_name, "eea_fn_adc1_get_raw", "i(i*)", &eea_fn_adc1_get_raw);
}