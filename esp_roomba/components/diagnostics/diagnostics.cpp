// diagnostics.cpp

extern "C" {
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// For CPU frequency from sdkconfig:
#include "sdkconfig.h"
}

#include "diagnostics.hpp"

static const char *TAG = "system_monitor";

// Monitor handles
static temperature_sensor_handle_t temp_sensor = nullptr;
static adc_oneshot_unit_handle_t adc1_handle = nullptr;
static adc_cali_handle_t adc_cali_hdl = nullptr;

// --------------------- ADC Initialization ---------------------
static auto init_adc() -> esp_err_t {
  // Minimal one-shot config
  adc_oneshot_unit_init_cfg_t init_cfg = {
    .unit_id = ADC_UNIT_1, .clk_src = ADC_RTC_CLK_SRC_DEFAULT, .ulp_mode = ADC_ULP_MODE_DISABLE,
    // .clk_src = ADC_ONESHOT_CLK_SRC_DEFAULT, // Uncomment if desired
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

  // Channel config
  adc_oneshot_chan_cfg_t chan_cfg = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  // ADC_CHANNEL_0 => GPIO1 on ESP32-S3, if not physically connected to anything,
  // you will read 0 or random values
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &chan_cfg));

  // One-shot calibration
  adc_cali_curve_fitting_config_t cali_cfg = {
    .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT,
    // .chan   = ADC_CHANNEL_0, // optional if you want per-channel calibration
  };
  ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &adc_cali_hdl));

  return ESP_OK;
}

// --------------------- Temperature Initialization ---------------------
static auto init_temp_sensor() -> esp_err_t {
  // Valid range on ESP32-S3 is ~20 to 90 °C
  temperature_sensor_config_t temp_cfg = {
    .range_min = 20, .range_max = 90, .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT, .flags = {0}};

  esp_err_t err = temperature_sensor_install(&temp_cfg, &temp_sensor);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install temperature sensor: %s", esp_err_to_name(err));
    return err;
  }
  ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

  return ESP_OK;
}

// --------------------- Monitor Initialization ---------------------
auto init_system_monitor() -> esp_err_t {
  ESP_ERROR_CHECK(init_temp_sensor());
  ESP_ERROR_CHECK(init_adc());
  return ESP_OK;
}

// --------------------- Populate System Status ---------------------
auto get_system_status(system_status_t *status) -> esp_err_t {
  if (status == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }

  // Initialize memory fields to safe values
  status->total_internal_heap = 0;
  status->free_internal_heap = 0;
  status->total_psram = 0;
  status->free_psram = 0;
  status->free_stack = 0;

  // 1) Temperature
  ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &status->temperature));

  // 2) ADC-based voltage
  //    If ADC_CHANNEL_0 isn't connected, you'll see ~0.0 V
  int raw_val = 0;
  int mv = 0;
  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw_val));
  ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_hdl, raw_val, &mv));
  status->voltage = mv / 1000.0F;  // convert mV -> V

  // 3) Free heap (size_t, so cast to unsigned long for logging)
  status->free_heap = esp_get_free_heap_size();

  // 4) Wi-Fi TX power, if Wi-Fi is running
  //    The raw value is in quarter dBm, so 80 = 20 dBm
  int8_t raw_tx_power_qdbm = 0;
  if (esp_wifi_get_max_tx_power(&raw_tx_power_qdbm) == ESP_OK) {
    status->wifi_tx_power = raw_tx_power_qdbm;  // store quarter dBm for now
  } else {
    status->wifi_tx_power = -1;
  }

  // 5) CPU frequency (MHz) from sdkconfig
  status->cpu_frequency = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

  // 6) Wi-Fi bandwidth
  uint8_t primary_ch;
  wifi_second_chan_t second_ch;
  if (esp_wifi_get_channel(&primary_ch, &second_ch) == ESP_OK) {
    status->wifi_bandwidth = (second_ch == WIFI_SECOND_CHAN_NONE) ? WIFI_BW_HT20 : WIFI_BW_HT40;
  } else {
    status->wifi_bandwidth = WIFI_BW_HT20;  // fallback
  }

  // Add memory monitoring
  status->total_internal_heap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  status->free_internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  status->total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  status->free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  status->free_stack = uxTaskGetStackHighWaterMark(NULL);

  return ESP_OK;
}

// --------------------- Print System Status ---------------------
auto print_system_status(const system_status_t *status) -> void {
  if (!status) {
    return;
  }

  // Convert quarter dBm to normal dBm
  float wifi_power_dbm = status->wifi_tx_power / 4.0f;  // e.g. 80 => 20.0 dBm

  // System Status
  ESP_LOGI(TAG, "=== System Status ===");
  ESP_LOGI(TAG, "Temperature: %.2f °C", status->temperature);
  ESP_LOGI(TAG, "System Voltage: %.3f V", status->voltage);
  ESP_LOGI(TAG, "Free Heap: %lu bytes", (unsigned long)status->free_heap);
  ESP_LOGI(TAG, "Wi-Fi TX Power: %.1f dBm", wifi_power_dbm);
  ESP_LOGI(TAG, "CPU Frequency: %lu MHz", (unsigned long)status->cpu_frequency);
  ESP_LOGI(TAG, "Wi-Fi Bandwidth: %s", (status->wifi_bandwidth == WIFI_BW_HT40) ? "40MHz" : "20MHz");

  // Memory Status
  ESP_LOGI(TAG, "=== Memory Status ===");
  ESP_LOGI(
    TAG,
    "Internal Heap: %lu/%lu bytes",
    (unsigned long)status->free_internal_heap,
    (unsigned long)status->total_internal_heap);

  if (status->total_psram > 0) {
    ESP_LOGI(TAG, "PSRAM: %lu/%lu bytes", (unsigned long)status->free_psram, (unsigned long)status->total_psram);
  }

  if (status->free_stack > 0) {
    ESP_LOGI(TAG, "Stack Free: %lu bytes", (unsigned long)status->free_stack);
  }

  // Warnings
  if (status->temperature > 80.0F) {
    ESP_LOGW(TAG, "WARNING: High temperature detected!");
  }
  if (status->free_heap < 10000) {
    ESP_LOGW(TAG, "WARNING: Low memory!");
  }
  if (status->free_stack < 1024) {
    ESP_LOGW(TAG, "WARNING: Low stack space!");
  }
}

// --------------------- Cleanup Monitor ---------------------
auto cleanup_system_monitor() -> void {
  if (temp_sensor != nullptr) {
    temperature_sensor_disable(temp_sensor);
    temperature_sensor_uninstall(temp_sensor);
    temp_sensor = nullptr;
  }
  if (adc1_handle != nullptr) {
    adc_oneshot_del_unit(adc1_handle);
    adc1_handle = nullptr;
  }
  if (adc_cali_hdl != nullptr) {
    adc_cali_delete_scheme_curve_fitting(adc_cali_hdl);
    adc_cali_hdl = nullptr;
  }
}
