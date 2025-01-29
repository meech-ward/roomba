

#include "camera.hpp"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include <cstring>

#include "camera_config.hpp"

namespace camera {

static const char* TAG = "camera";

static SemaphoreHandle_t s_fb_mutex;

// normal image buffer
static uint8_t* s_jpeg_buffer = nullptr;
// copy of image buffer for another thread
static uint8_t* s_jpeg_buffer_copy = nullptr;
// arbitrary buffer size that works for camera settings
// done this way to only heap allocate once at setup
static constexpr size_t s_jpeg_buffer_len = 128 * 1024;
static size_t jpeg_len = 0;
static uint64_t s_jpeg_timestamp = 0;

static camera_config_t camera_config = {
  .pin_pwdn = CAM_PIN_PWDN,
  .pin_reset = CAM_PIN_RESET,
  .pin_xclk = CAM_PIN_XCLK,
  .pin_sccb_sda = CAM_PIN_SIOD,
  .pin_sccb_scl = CAM_PIN_SIOC,

  .pin_d7 = CAM_PIN_D7,
  .pin_d6 = CAM_PIN_D6,
  .pin_d5 = CAM_PIN_D5,
  .pin_d4 = CAM_PIN_D4,
  .pin_d3 = CAM_PIN_D3,
  .pin_d2 = CAM_PIN_D2,
  .pin_d1 = CAM_PIN_D1,
  .pin_d0 = CAM_PIN_D0,
  .pin_vsync = CAM_PIN_VSYNC,
  .pin_href = CAM_PIN_HREF,
  .pin_pclk = CAM_PIN_PCLK,

  .xclk_freq_hz = 20000000,  // 20000000,        // The clock frequency of the image sensor
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_JPEG,  // The pixel format of the image: PIXFORMAT_ + YUV422|GRAYSCALE|RGB565|JPEG
  .frame_size = FRAMESIZE_VGA,  // FRAMESIZE_SVGA, // 800x600       // FRAMESIZE_SVGA,       // FRAMESIZE_HD (works but
                                // it's intensive over wifi), //
                                //   FRAMESIZE_SVGA,    // FRAMESIZE_XGA,    // FRAMESIZE_SVGA, //FRAMESIZE_UXGA,
                                //    //FRAMESIZE_SVGA, FRAMESIZE_UXGA //
                                //      FRAMESIZE_QQVGA, //, // ,       // FRAMESIZE_VGA,  // FRAMESIZE_QVGA,    //
                                //       FRAMESIZE_VGA,     // FRAMESIZE_UXGA,
                                //       //FRAMESIZE_UXGA, // The
                                //        resolution
                                //         size of the image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  .jpeg_quality = 8,                  // The quality of the JPEG image, ranging from 0 to 63.
  .fb_count = 4,                      // The number of frame buffers to use.
  .fb_location = CAMERA_FB_IN_PSRAM,  // Set the frame buffer storage location
  .grab_mode = CAMERA_GRAB_LATEST,    //  The image capture mode.
                                      // .sccb_i2c_port = 0,                 // Explicitly set I2C port
  .sccb_i2c_port = 1,                 // You're using I2C port 1

};

static auto init_camera() -> esp_err_t {
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera Init Failed");
    return err;
  }

  return ESP_OK;
}

void setup() {
  // s_jpeg_buffer = (uint8_t*)heap_caps_malloc(s_jpeg_buffer_len, MALLOC_CAP_SPIRAM);
  // s_jpeg_buffer_copy = (uint8_t*)heap_caps_malloc(s_jpeg_buffer_len, MALLOC_CAP_SPIRAM);
  s_jpeg_buffer = (uint8_t*)malloc(s_jpeg_buffer_len);
  s_jpeg_buffer_copy = (uint8_t*)malloc(s_jpeg_buffer_len);

  if (ESP_OK != init_camera()) {
    ESP_LOGE(TAG, "Camera Init Failed");
    return;
  }

  s_fb_mutex = xSemaphoreCreateMutex();
}
auto copy_jpeg_buffer() -> JpegBuffer {
  static constexpr TickType_t xTicksToWait = pdMS_TO_TICKS(3000);  // portMAX_DELAY
  if (xSemaphoreTake(s_fb_mutex, xTicksToWait) != pdTRUE) {
    return JpegBuffer{nullptr, 0, 0};
  }

  memcpy(s_jpeg_buffer_copy, s_jpeg_buffer, jpeg_len);
  xSemaphoreGive(s_fb_mutex);
  return JpegBuffer{s_jpeg_buffer_copy, jpeg_len, s_jpeg_timestamp};
}

void camera_capture_task(void* arg) {
  while (true) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {  // Add explicit check for null
      ESP_LOGE(TAG, "Failed to get camera frame");
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (fb->len <= 0 || fb->buf == nullptr) {
      ESP_LOGE(TAG, "Invalid frame buffer: len=%d, buf=%p", fb->len, fb->buf);
      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Check for valid JPEG
    if (fb->len < JPEG_HEADER_SIZE || fb->buf[0] != JPEG_SOI_MARKER_FIRST || fb->buf[1] != JPEG_SOI_MARKER_SECOND) {
      ESP_LOGE(TAG, "Invalid JPEG data: first bytes: %02x %02x", fb->buf[0], fb->buf[1]);
      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Add size check against buffer
    if (fb->len > s_jpeg_buffer_len) {
      ESP_LOGE(TAG, "Frame too large: %d > %d", fb->len, s_jpeg_buffer_len);
      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (xSemaphoreTake(s_fb_mutex, portMAX_DELAY) != pdTRUE) {
      ESP_LOGE(TAG, "Failed to take mutex");
      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Add null check for buffer before copy
    if (s_jpeg_buffer == nullptr) {
      ESP_LOGE(TAG, "JPEG buffer is null");
      xSemaphoreGive(s_fb_mutex);
      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    memcpy(s_jpeg_buffer, fb->buf, fb->len);
    jpeg_len = fb->len;
    s_jpeg_timestamp = esp_timer_get_time();

    xSemaphoreGive(s_fb_mutex);
    esp_camera_fb_return(fb);

    // needs to be tweaked, not entirely sure why or when
    // depends on a lot of factors
    // make sure you update the ws delay too
    // vTaskDelay(pdMS_TO_TICKS(30)); // 5640
    vTaskDelay(pdMS_TO_TICKS(10));  // 2640
  }
}

}  // namespace camera
