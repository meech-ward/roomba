#include "server_integration.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "camera.hpp"
#include "esp_http_server.h"
#include "esp_log_level.h"
#include "esp_timer.h"
#include "motor_command.hpp"

static const char* TAG = "server_integration";

static bool s_streaming = false;
static int s_ws_fd = -1;

using namespace server;

// 34048 ov5640
// 17628 ov2640

constexpr auto max_buf_size_to_send = 16384;
// constexpr auto prefered_loop_duration_us = 120 * 1000;  // ov5640
constexpr auto prefered_loop_duration_us = 60 * 1000;  // ov2640
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
    if (!s_streaming || s_ws_fd < 0) {
      printf("No streaming %d, %d \n", static_cast<int>(s_streaming), s_ws_fd);
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
      // s_streaming = false;
      // s_ws_fd = -1;
      // actually just give it a break
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}


auto handle_text_message(httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void {
  if (strcmp((char*)buf, "start") == 0) {
    ESP_LOGI(TAG, "Received 'start' => begin streaming");
    s_streaming = true;
    s_ws_fd = fd;  // store the single client's socket
    return;
  }
  if (strcmp((char*)buf, "stop") == 0) {
    ESP_LOGI(TAG, "Received 'stop' => stop streaming");
    s_streaming = false;
    s_ws_fd = -1;
    return;
  }

  ESP_LOGI(TAG, "Received unknown msg: %s", buf);
}

auto handle_binary_message(httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void {
  if (ws_pkt.len != MotorCommand::data_size) {
    ESP_LOGW(TAG, "Invalid binary packet size: %d", ws_pkt.len);
    return;
  }
  write_motor_data(buf);
}
