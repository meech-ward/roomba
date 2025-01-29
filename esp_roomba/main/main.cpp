#include <esp_log.h>
#include <nvs_flash.h>

#include "camera.hpp"
#include "diagnostics.hpp"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "motor_command.hpp"
#include "new_socket_server.hpp"
#include "server_integration.hpp"
#include "wifi_ap.hpp"
#include "wifi_manager.hpp"

static const char* TAG = "Main";

constexpr size_t camStackSize = 6144;
constexpr size_t streamStackSize = 8192;
constexpr size_t motorStackSize = 4096;
constexpr size_t captureTaskPriority = configMAX_PRIORITIES - 5;
constexpr size_t motorTaskPriority = configMAX_PRIORITIES - 3;
constexpr size_t streamTaskPriority = configMAX_PRIORITIES - 4;

static StaticTask_t streamTaskBuffer;
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
static StackType_t streamTaskStack[streamStackSize / sizeof(StackType_t)];
static StaticTask_t captureTaskBuffer;
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
static StackType_t captureTaskStack[camStackSize / sizeof(StackType_t)];
static StaticTask_t motorTaskBuffer;
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
static StackType_t motorTaskStack[motorStackSize / sizeof(StackType_t)];

static auto init_nvs() -> void {
  auto ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
}

static httpd_handle_t ws_server = nullptr;
static auto setup_wifi_connect() -> void;
extern "C" void app_main() {
  init_nvs();
  // setup_wifi();
  setup_wifi_connect();
  auto& wifi = wifi::WifiManager::instance();

  while (!wifi.wifi_ready) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  vTaskDelay(pdMS_TO_TICKS(1000));  // Give WiFi time to stabilize

  camera::setup();

  server::set_ws_binary_handler(handle_binary_message);
  server::set_ws_text_handler(handle_text_message);
  ws_server = server::start_webserver();

  write_motor_data_zero();

  // constantly capturing for now, maybe turn this on and off later
  TaskHandle_t captureTaskHandle = xTaskCreateStaticPinnedToCore(
    camera::camera_capture_task,
    "camera_capture_task",
    camStackSize / sizeof(StackType_t),
    nullptr,
    captureTaskPriority,
    captureTaskStack,
    &captureTaskBuffer,
    1);

  if (captureTaskHandle == nullptr) {
    ESP_LOGE(TAG, "Failed to create capture task");
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  // constantly update the state of the motors
  TaskHandle_t motorTaskHandle = xTaskCreateStaticPinnedToCore(
    motor_control_task,
    "motor_control_task",
    motorStackSize / sizeof(StackType_t),
    nullptr,
    motorTaskPriority,
    motorTaskStack,
    &motorTaskBuffer,
    1);

  if (motorTaskHandle == nullptr) {
    ESP_LOGE(TAG, "Failed to create motor task");
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  // constatly look out for a ws client to stream to
  TaskHandle_t streamTaskHandle = xTaskCreateStaticPinnedToCore(
    camera_stream_task,
    "cam_stream_task",
    streamStackSize / sizeof(StackType_t),
    ws_server,
    streamTaskPriority,
    streamTaskStack,
    &streamTaskBuffer,
    0);

  if (streamTaskHandle == nullptr) {
    ESP_LOGE(TAG, "Failed to create stream task");
    return;
  }
  // });

#ifndef NDEBUG
  esp_log_level_set("*", ESP_LOG_DEBUG);
  ESP_LOGI(TAG, "Starting System Monitor Example...");
  auto ret = init_system_monitor();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "System monitor init failed: %s", esp_err_to_name(ret));
    return;
  }
#endif

  while (true) {
#ifndef NDEBUG
    system_status_t current_status = {};
    if (get_system_status(&current_status) == ESP_OK) {
      print_system_status(&current_status);
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(15000));
  }
}

// connect to wifi
void setup_wifi_connect() {
  auto& wifi = wifi::WifiManager::instance();
  // wifi.initialize_for_scan();
  // wifi.scan_networks();
  // return;

  // Configure WiFi
  wifi::WifiConfig config{
    .ssid = "",
    .password = "",
    .connection_timeout = std::chrono::milliseconds(15000),
    .max_retries = 5};

  // Initialize
  if (auto result = wifi.initialize(config); !result) {
    ESP_LOGE("WIFI", "Failed to initialize WiFi");
    return;
  }

  // Set up callbacks
  wifi.on_connected([](const wifi::ConnectionInfo& info) {
    printf("connected to wifi: %li\n", (long unsigned int)info.ip_info.ip.addr);

    if (info.ip_info.ip.addr == 0) {
      ESP_LOGE("WIFI", "Failed to get IP address");
      return;
    }

    ESP_LOGI("WIFI", "Connected to WiFi! IP: " IPSTR, IP2STR(&info.ip_info.ip));
  });

  wifi.on_disconnected([](wifi::WifiError error) { ESP_LOGW("WIFI", "Disconnected from WiFi"); });

  // Connect
  if (auto result = wifi.connect(); !result) {
    ESP_LOGE("WIFI", "Failed to connect to WiFi");
    return;
  }
}