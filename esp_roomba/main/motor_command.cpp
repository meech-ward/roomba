#include "motor_command.hpp"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

#include "esp_log.h"
#include "esp_system.h"
#include "motor.hpp"

static const char* TAG = "motor_control";
static constexpr int delay_ms = 10;
// 400ms in microseconds, the amount of time without a command before halting the motors
static constexpr uint64_t timout_micros = 400000;

static MotorCommand command;
static std::atomic<uint64_t> sequence{0};
// left
static gpio::Motor motor1{GPIO_NUM_5, GPIO_NUM_3, GPIO_NUM_4, LEDC_CHANNEL_0};
// right
static gpio::Motor motor2{GPIO_NUM_6, GPIO_NUM_8, GPIO_NUM_9, LEDC_CHANNEL_1};
// vaccum & brush
static gpio::Motor motor3{GPIO_NUM_7, GPIO_NUM_44, GPIO_NUM_43, LEDC_CHANNEL_2};

auto write_motor_data_zero() -> void {
  memset(&command, 0, sizeof(MotorCommand));
}
auto write_motor_data(const uint8_t* data) -> void {
  memcpy(&command.speeds, data, MotorCommand::data_size);

  // Update metadata
  command.sequence = sequence.fetch_add(1);
  command.timestamp = esp_timer_get_time();
}

static auto read_motor_data(MotorCommand& output, uint64_t last_sequence) -> bool {
  memcpy(&output, &command, sizeof(MotorCommand));
  return output.sequence > last_sequence;
}

static void stop_motors() {
  auto m1Result = motor1.stop();
  if (!m1Result) {
    ESP_LOGE(TAG, "Motor 1 stop failed with error code: %d", static_cast<int>(m1Result.error()));
    esp_restart();
  }
  auto m2Result = motor2.stop();
  if (!m2Result) {
    ESP_LOGE(TAG, "Motor 2 stop failed with error code: %d", static_cast<int>(m2Result.error()));
    esp_restart();
  }
  auto m3Result = motor3.stop();
  if (!m3Result) {
    ESP_LOGE(TAG, "Motor 3 stop failed with error code: %d", static_cast<int>(m3Result.error()));
    esp_restart();
  }
}

// Motor control task
void motor_control_task(void* arg) {
  const TickType_t interval = pdMS_TO_TICKS(1);  // 1ms interval

  MotorCommand current;
  uint64_t last_sequence = 0;
  TickType_t lastWakeTime = xTaskGetTickCount();

  auto m1Result = motor1.init();
  if (!m1Result) {
    ESP_LOGE(TAG, "Motor 1 init failed with error code: %d", static_cast<int>(m1Result.error()));
  }
  auto m2Result = motor2.init();
  if (!m2Result) {
    ESP_LOGE(TAG, "Motor 2 init failed with error code: %d", static_cast<int>(m2Result.error()));
  }
  auto m3Result = motor3.init();
  if (!m3Result) {
    ESP_LOGE(TAG, "Motor 3 init failed with error code: %d", static_cast<int>(m3Result.error()));
  }

  stop_motors();

  while (true) {
    bool got_new_command = read_motor_data(current, last_sequence);

    // If there's no instructions for 400ms, stop motors
    uint64_t now = esp_timer_get_time();
    if (now - current.timestamp > timout_micros) {
      stop_motors();
      vTaskDelay(delay_ms);
      continue;
    }

    if (!got_new_command) {
      vTaskDelay(delay_ms);
      continue;
    }

    bool m1IsForward = current.getDirection(0);
    bool m2IsForward = current.getDirection(1);
    bool m3IsForward = current.getDirection(2);
    auto m1Result =
      m1IsForward ? motor1.forward(current.getScaledSpeed(0)) : motor1.backward(current.getScaledSpeed(0));
    if (!m1Result) {
      ESP_LOGE(TAG, "Motor 1 forward failed with error code: %d", static_cast<int>(m1Result.error()));
    }

    auto m2Result =
      m2IsForward ? motor2.forward(current.getScaledSpeed(1)) : motor2.backward(current.getScaledSpeed(1));
    if (!m2Result) {
      ESP_LOGE(TAG, "Motor 2 forward failed with error code: %d", static_cast<int>(m2Result.error()));
    }

    auto m3Result =
      m3IsForward ? motor3.forward(current.getScaledSpeed(2)) : motor3.backward(current.getScaledSpeed(2));
    if (!m3Result) {
      ESP_LOGE(TAG, "Motor 3 forward failed with error code: %d", static_cast<int>(m3Result.error()));
    }

    ESP_LOGI(
      TAG,
      "Motors: M1=%d%% %s, M2=%d%% %s, M3=%d%% %s",
      current.speeds[0],
      m1IsForward ? "FWD" : "REV",
      current.speeds[1],
      m2IsForward ? "FWD" : "REV",
      current.speeds[2],
      m3IsForward ? "FWD" : "REV");

    last_sequence = current.sequence;

    if (interval > 0) {
      vTaskDelayUntil(&lastWakeTime, interval);
    } else {
      vTaskDelay(delay_ms);
    }
  }
}
