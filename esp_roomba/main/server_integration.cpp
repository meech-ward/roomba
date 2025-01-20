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

constexpr auto max_buf_size_to_send = 50000;

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
  while (true) {
    if (!s_streaming || s_ws_fd < 0) {
      printf("No streaming %d, %d \n", static_cast<int>(s_streaming), s_ws_fd);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < max_buf_size_to_send) {
      ESP_LOGW(TAG, "Low memory, skipping frame");
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    auto jpeg_buffer = camera::copy_jpeg_buffer();
    if (
      jpeg_buffer.buffer == nullptr || jpeg_buffer.len < 2 ||
      jpeg_buffer.buffer[0] != camera::JPEG_SOI_MARKER_FIRST ||
      jpeg_buffer.buffer[1] != camera::JPEG_SOI_MARKER_SECOND) {
      ESP_LOGW(TAG, "Invalid JPEG data");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (jpeg_buffer.timestamp == prev_timestamp) {
      continue;
    }
    prev_timestamp = jpeg_buffer.timestamp;

    // Prepare a WS frame
    ws_pkt.payload = jpeg_buffer.buffer;
    ws_pkt.len = jpeg_buffer.len;

    // Send asynchronously
    // esp_err_t err = httpd_ws_send_frame_async(s_server, s_ws_fd, &ws_pkt);
    // send sync and block, seems to work better with no need to worry about backign up the queue
    esp_err_t err = httpd_ws_send_data(s_server, s_ws_fd, &ws_pkt);

    // 1 was so laggy beause of the frames just fucking piling up like a bitch
    // 60 almost seams like 24 ish fps
    // that was true until i did more things now 30 seems good
    // not entirely sure why
    vTaskDelay(pdMS_TO_TICKS(30));

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
