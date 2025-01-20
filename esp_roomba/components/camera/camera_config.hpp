#pragma once

/**
 * Kconfig setup
 *
 * If you have a Kconfig file, copy the content from
 *  https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
 * In case you haven't, copy and paste this Kconfig file inside the src directory.
 * This Kconfig file has definitions that allows more control over the camera and
 * how it will be initialized.
 */

/**
 * Enable PSRAM on sdkconfig:
 *
 * CONFIG_ESP32_SPIRAM_SUPPORT=y
 *
 * More info on
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
 */

// ================================ CODE ======================================


// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

namespace camera {
#define BOARD_SEEED_XIAO_ESP32S3_SENSE 1
// #define BOARD_SEEED_XIAO_ESP32S3_SENSE_MOCK 1

// WROVER-KIT PIN Map
#ifdef BOARD_WROVER_KIT

#define CAM_PIN_PWDN -1   // power down is not used
#define CAM_PIN_RESET -1  // software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1  // software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif
// ESP32S3 (WROOM) PIN Map
#ifdef BOARD_ESP32S3_WROOM
#define CAM_PIN_PWDN 38
#define CAM_PIN_RESET -1  // software reset will be performed
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16
#endif

#ifdef BOARD_SEEED_XIAO_ESP32S3_SENSE

// Power pins (if not used, set to -1):
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1

// Clock signals
#define CAM_PIN_XCLK 10  // XMCLK

// I2C lines
#define CAM_PIN_SIOC 39  // CAM_SCL
#define CAM_PIN_SIOD 40  // CAM_SDA

// Parallel DVP data lines (least significant bit first)
#define CAM_PIN_D0 15  // DVP_Y2
#define CAM_PIN_D1 17  // DVP_Y3
#define CAM_PIN_D2 18  // DVP_Y4
#define CAM_PIN_D3 16  // DVP_Y5
#define CAM_PIN_D4 14  // DVP_Y6
#define CAM_PIN_D5 12  // DVP_Y7
#define CAM_PIN_D6 11  // DVP_Y8
#define CAM_PIN_D7 48  // DVP_Y9

// Synchronization signals
#define CAM_PIN_VSYNC 38  // DVP_VSYNC
#define CAM_PIN_HREF 47   // DVP_HREF
#define CAM_PIN_PCLK 13   // DVP_PCLK

#endif

#ifdef BOARD_SEEED_XIAO_ESP32S3_SENSE_MOCK

// Power pins (if not used, set to -1):
#define CAM_PIN_PWDN (-1)
#define CAM_PIN_RESET (-1)

// Clock signals
#define CAM_PIN_XCLK 10  // XMCLK

// I2C lines
#define CAM_PIN_SIOC 39  // CAM_SCL
#define CAM_PIN_SIOD 40  // CAM_SDA

// Parallel DVP data lines (least significant bit first)
#define CAM_PIN_D0 15  // DVP_Y2
#define CAM_PIN_D1 17  // DVP_Y3
#define CAM_PIN_D2 18  // DVP_Y4
#define CAM_PIN_D3 16  // DVP_Y5
#define CAM_PIN_D4 14  // DVP_Y6
#define CAM_PIN_D5 12  // DVP_Y7
#define CAM_PIN_D6 11  // DVP_Y8
#define CAM_PIN_D7 21  // DVP_Y9

// Synchronization signals
#define CAM_PIN_VSYNC 38  // DVP_VSYNC
#define CAM_PIN_HREF 47   // DVP_HREF
#define CAM_PIN_PCLK 13   // DVP_PCLK

#endif
}  // namespace camera