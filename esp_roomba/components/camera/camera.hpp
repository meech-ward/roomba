#pragma once

#include <tuple>

#include "esp_camera.h"
#include "freertos/FreeRTOS.h"

namespace camera {
constexpr uint8_t JPEG_HEADER_SIZE = 2;
constexpr uint8_t JPEG_SOI_MARKER_FIRST = 0xFF;
constexpr uint8_t JPEG_SOI_MARKER_SECOND = 0xD8;

struct JpegBuffer {
  uint8_t* buffer;
  size_t len;
  uint64_t timestamp;
};

auto setup() -> void;
auto camera_capture_task(void* arg) -> void;

// safe to call from another thread
auto copy_jpeg_buffer() -> JpegBuffer;

}  // namespace camera
