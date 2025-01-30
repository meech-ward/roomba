#include "server_integration.hpp"

#include <cJSON.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "camera.hpp"
#include "esp_http_server.h"
#include "esp_log_level.h"
#include "esp_timer.h"
#include "motor_command.hpp"

static const char* TAG = "server_integration";

static bool s_camera_streaming = false;
static int s_ws_fd = -1;
static Roomba* s_roomba = nullptr;
static bool s_sensor_streaming = false;

using namespace server;

// 34048 ov5640
// 17628 ov2640

constexpr auto max_buf_size_to_send = 16384;
// constexpr auto prefered_loop_duration_us = 120 * 1000;  // ov5640
constexpr auto prefered_loop_duration_us = 40 * 1000;  // ov2640
auto camera_stream_task(void* arg) -> void {
  auto* s_server = static_cast<httpd_handle_t>(arg);
  ESP_LOGW(TAG, "Start Stream");

  // Pre-allocate the frame structure outside the loop
  static httpd_ws_frame_t ws_pkt = {
    .final = true,  // If this is always true
    .fragmented = false,
    .type = HTTPD_WS_TYPE_BINARY,  // Set the constant values once
    .payload = nullptr,            // Will update this per frame
    .len = 0,                      // Will update this per frame
  };
  uint32_t prev_timestamp = 0;
  uint64_t end_of_loop_time = esp_timer_get_time();
  uint64_t last_loop_time = esp_timer_get_time();
  while (true) {
    uint64_t current_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Time since last loop: %llu us", current_time - last_loop_time);
    last_loop_time = current_time;
    if (!s_camera_streaming || s_ws_fd < 0) {
      printf("No streaming %d, %d \n", static_cast<int>(s_camera_streaming), s_ws_fd);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < max_buf_size_to_send) {
      ESP_LOGW(TAG, "Low memory, skipping frame");
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    auto jpeg_buffer = camera::copy_jpeg_buffer();
    if (
      jpeg_buffer.buffer == nullptr || jpeg_buffer.len < 2 || jpeg_buffer.buffer[0] != camera::JPEG_SOI_MARKER_FIRST ||
      jpeg_buffer.buffer[1] != camera::JPEG_SOI_MARKER_SECOND) {
      ESP_LOGW(TAG, "Invalid JPEG data");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (jpeg_buffer.timestamp == prev_timestamp) {
      // Make sure we don't delay for 0
      ESP_LOGW(TAG, "Duplicate JPEG data");
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    prev_timestamp = jpeg_buffer.timestamp;

    // Prepare a WS frame
    ws_pkt.payload = jpeg_buffer.buffer;
    ws_pkt.len = jpeg_buffer.len;
    ESP_LOGI(TAG, "JPEG length: %zu bytes", ws_pkt.len);

    // Send asynchronously
    // esp_err_t err = httpd_ws_send_frame_async(s_server, s_ws_fd, &ws_pkt);
    // send sync and block, seems to work better with no need to worry about backign up the queue
    uint64_t send_start = esp_timer_get_time();
    esp_err_t err = httpd_ws_send_data(s_server, s_ws_fd, &ws_pkt);
    uint64_t send_time = esp_timer_get_time() - send_start;
    if (send_time > 100000) {  // Log if send takes >100ms
      ESP_LOGW(TAG, "Long send time: %llu us", send_time);
    }
    // try to level out how often the frame is sent
    uint64_t new_time = esp_timer_get_time();
    auto elapsed_us = new_time - end_of_loop_time;
    // Use microsecond precision by working in micros until the last moment
    if (elapsed_us < prefered_loop_duration_us) {
      int delay_us = prefered_loop_duration_us - elapsed_us;
      // Convert to milliseconds at the last moment, rounding up
      int32_t delay_ms = (delay_us + 999) / 1000;  // This rounds up
      if (delay_ms > 0) {                          // Make sure we don't delay for 0
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    end_of_loop_time = esp_timer_get_time();

    if (err != ESP_OK) {
      // Typically means the client disconnected or send error
      ESP_LOGW(TAG, "WS send failed: %d. Stopping stream.", err);
      // s_camera_streaming = false;
      // s_ws_fd = -1;
      // actually just give it a break
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

static httpd_handle_t s_server = nullptr;

// Modify handleSensorData to be more robust
static void handleSensorData(const Roomba::StreamPacket& packet) {
  if (!s_sensor_streaming || s_ws_fd < 0) {
    return;  // Don't process if we're not supposed to be streaming
  }

  if (s_server == nullptr) {
    ESP_LOGE(TAG, "Server not initialized");
    return;
  }

  // Prepare WebSocket frame with the raw StreamPacket data
  httpd_ws_frame_t ws_pkt = {
    .final = true,
    .fragmented = false,
    .type = HTTPD_WS_TYPE_BINARY,
    .payload = const_cast<uint8_t*>(packet.data.data()),
    .len = packet.data.size()};

  // Try sending synchronously instead of async
  esp_err_t ret = httpd_ws_send_data(s_server, s_ws_fd, &ws_pkt);
  if (ret != ESP_OK) {
    if (ret == ESP_ERR_INVALID_STATE || ret == ESP_ERR_INVALID_ARG) {
      // Connection is probably dead
      ESP_LOGW(TAG, "WebSocket connection appears dead, stopping sensor stream");
      s_sensor_streaming = false;
      s_ws_fd = -1;
    } else {
      // Other errors - just log and continue
      ESP_LOGW(TAG, "Failed to send sensor data: %d", ret);
      vTaskDelay(pdMS_TO_TICKS(100));  // Short delay before next attempt
    }
  }
}

// Add connection status check
static auto is_ws_connected() -> bool {
  return s_ws_fd >= 0 && s_server != nullptr;
}

static void stop_streaming_sensors();

// Update start_streaming_sensors to check connection
static void start_streaming_sensors(httpd_handle_t& server) {
  s_server = server;
  if (!is_ws_connected()) {
    ESP_LOGE(TAG, "Cannot start streaming - no WebSocket connection");
    return;
  }

  // Define the sensors we want to stream
  std::vector<Roomba::SensorPacket> packets = {
    Roomba::SensorPacket::Bumps,      // Crash bumper (1 byte)
    Roomba::SensorPacket::CliffLeft,  // Cliff sensors (1 byte each)
    Roomba::SensorPacket::CliffFrontLeft,
    Roomba::SensorPacket::CliffFrontRight,
    Roomba::SensorPacket::CliffRight,
    Roomba::SensorPacket::Voltage,            // Battery voltage (2 bytes)
    Roomba::SensorPacket::Current,            // Current (2 bytes)
    Roomba::SensorPacket::Temperature,        // Temperature (1 byte)
    Roomba::SensorPacket::LeftMotorCurrent,   // Left motor current (2 bytes)
    Roomba::SensorPacket::RightMotorCurrent,  // Right motor current (2 bytes)
    Roomba::SensorPacket::LightBumper,        // Light bumper (1 byte)
  };

  // Stop any existing stream first
  stop_streaming_sensors();
  // Start new stream
  if (auto result = s_roomba->startStreaming(packets)) {
    if (auto task_result = s_roomba->startStreamTask(handleSensorData)) {
      s_sensor_streaming = true;  // Enable WebSocket streaming
      ESP_LOGI("APP", "Streaming started successfully");
    } else {
      ESP_LOGE("APP", "Failed to start stream task: %d", static_cast<int>(task_result.error()));
      // Clean up the stream if task failed
      auto result = s_roomba->stopStreaming();
      if (result) {
        ESP_LOGI("APP", "Streaming stopped successfully");
      } else {
        ESP_LOGE("APP", "Error stopping stream: %d", static_cast<int>(result.error()));
      }
    }
  } else {
    ESP_LOGE("APP", "Failed to start streaming: %d", static_cast<int>(result.error()));
  }
}

static void stop_streaming_sensors() {
  s_sensor_streaming = false;  // Disable WebSocket streaming
  if (s_roomba != nullptr) {
    if (auto result = s_roomba->stopStreamTask()) {
      ESP_LOGI("APP", "Streaming stopped successfully");
    } else {
      ESP_LOGW("APP", "Error stopping stream: %d", static_cast<int>(result.error()));
    }
  }
}

auto handle_text_message(httpd_handle_t& server, httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void {
  if (s_roomba == nullptr) {
    s_roomba = new Roomba(Roomba::Config());
    if (!s_roomba->isInitialized()) {
      ESP_LOGE(TAG, "Failed to initialize Roomba!");
      return;
    }
  }

  if (strcmp((char*)buf, "start") == 0) {
    ESP_LOGI(TAG, "Received 'start' => begin streaming");
    s_camera_streaming = true;
    s_ws_fd = fd;
    return;
  }
  if (strcmp((char*)buf, "stop") == 0) {
    ESP_LOGI(TAG, "Received 'stop' => stop streaming");
    s_camera_streaming = false;
    return;
  }

  // Now parse and use the JSON
  cJSON* json = cJSON_Parse((char*)buf);
  if (json == nullptr) {
    ESP_LOGW(TAG, "Failed to parse JSON");
    return;
  }
  // Work with your JSON here
  cJSON* display_text = cJSON_GetObjectItem(json, "display");
  if (display_text != nullptr) {
    ESP_LOGI(TAG, "Got value: %s", display_text->valuestring);
    if (auto result = s_roomba->writeToDisplay(display_text->valuestring); !result) {
      ESP_LOGE(TAG, "Failed to write to display: error %d", (int)result.error());
    }
    ESP_LOGI(TAG, "Display text written");
  }

  cJSON* mode = cJSON_GetObjectItem(json, "mode");
  if (mode != nullptr) {
    ESP_LOGI(TAG, "Got value: %d", mode->valueint);
    if (mode->valueint == 0) {
      if (auto result = s_roomba->setSafeMode(); !result) {
        ESP_LOGE(TAG, "Failed to set mode: error %d", (int)result.error());
      }
    } else if (mode->valueint == 1) {
      if (auto result = s_roomba->setFullMode(); !result) {
        ESP_LOGE(TAG, "Failed to set mode: error %d", (int)result.error());
      }
    }
    ESP_LOGI(TAG, "Mode set");
  }
  cJSON* play_song = cJSON_GetObjectItem(json, "playSong");
  if (play_song != nullptr) {
    ESP_LOGI(TAG, "Got play_song request");
    if (auto result = s_roomba->playInTheEnd(); !result) {
      ESP_LOGE(TAG, "Failed to play song: error %d", (int)result.error());
    }
    ESP_LOGI(TAG, "Song played");
  }
  cJSON* play_daft_punk = cJSON_GetObjectItem(json, "playDaftPunk");
  if (play_daft_punk != nullptr) {
    ESP_LOGI(TAG, "Got play_daft_punk request");
    if (auto result = s_roomba->playDaftPunkSong(); !result) {
      ESP_LOGE(TAG, "Failed to play song: error %d", (int)result.error());
    }
    ESP_LOGI(TAG, "Song played");
  }

  cJSON* play_crowd_pleaser_2 = cJSON_GetObjectItem(json, "playSong2");
  if (play_crowd_pleaser_2 != nullptr) {
    ESP_LOGI(TAG, "Got play_crowd_pleaser_2 request");
    if (auto result = s_roomba->playCrowdPleaserSong2(); !result) {
      ESP_LOGE(TAG, "Failed to play song: error %d", (int)result.error());
    }
    ESP_LOGI(TAG, "Song played");
  }

  cJSON* start = cJSON_GetObjectItem(json, "start");
  if (start != nullptr) {
    ESP_LOGI(TAG, "Got start request");
    if (auto result = s_roomba->start(); !result) {
      ESP_LOGE(TAG, "Failed to start: error %d", (int)result.error());
    }
    ESP_LOGI(TAG, "Start command sent");
  }

  cJSON* stream_sensors = cJSON_GetObjectItem(json, "streamSensors");
  if (stream_sensors != nullptr) {
    ESP_LOGI(TAG, "Got value: %d", stream_sensors->valueint);
    ESP_LOGI(TAG, "Got streamSensors request");
    if (stream_sensors->valueint == 1) {
      s_ws_fd = fd;
      stop_streaming_sensors();  // Stop any existing stream first
      start_streaming_sensors(server);
    } else if (stream_sensors->valueint == 0) {
      stop_streaming_sensors();
    }
  }

  cJSON_Delete(json);
}

auto handle_binary_message(httpd_handle_t& server, httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void {
  if (ws_pkt.len != MotorCommand::data_size) {
    ESP_LOGW(TAG, "Invalid binary packet size: %d", ws_pkt.len);
    return;
  }
  write_motor_data(buf);
}

void cleanup_server_integration() {
  if (s_roomba != nullptr) {
    stop_streaming_sensors();
    // ... other cleanup ...
  }
}

// Add cleanup for WebSocket disconnect
void handle_ws_disconnect(int fd) {
  if (s_ws_fd == fd) {
    ESP_LOGI(TAG, "WebSocket disconnected, stopping streams");
    s_camera_streaming = false;
    s_sensor_streaming = false;
    s_ws_fd = -1;
    stop_streaming_sensors();
  }
}
