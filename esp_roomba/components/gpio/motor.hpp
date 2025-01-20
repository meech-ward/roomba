#pragma once

#include <memory>

#include "driver/gpio.h"
#include "hal/ledc_types.h"
#include "pwm_controller.hpp"

namespace gpio {

enum class MotorError { PwmInitFailed, PwmSetFailed, GpioInitFailed };

class Motor {
 public:
  /**
   * @brief Constructor for Motor class
   *
   * @param ena_pin PWM-capable GPIO pin for motor enable
   * @param in1_pin Direction control GPIO pin 1
   * @param in2_pin Direction control GPIO pin 2
   * @param frequency PWM frequency in Hz (default 20000 to avoid audible noise)
   */
  Motor(gpio_num_t ena_pin, gpio_num_t in1_pin, gpio_num_t in2_pin, ledc_channel_t channel, uint32_t frequency = 20000);

  /**
   * @brief Initialize the motor
   *
   * @return std::expected<void, MotorError> Success or error code
   */
  [[nodiscard]]
  auto init() -> std::expected<void, MotorError>;

  /**
   * @brief Set motor to move forward
   *
   * @param speed Speed percentage (0-100)
   * @return std::expected<void, MotorError> Success or error code
   */
  [[nodiscard]]
  auto forward(uint16_t speed) -> std::expected<void, MotorError>;

  /**
   * @brief Set motor to move backward
   *
   * @param speed Speed percentage (0-100)
   * @return std::expected<void, MotorError> Success or error code
   */
  [[nodiscard]]
  auto backward(uint16_t speed) -> std::expected<void, MotorError>;

  /**
   * @brief Stop the motor
   *
   * @return std::expected<void, MotorError> Success or error code
   */
  [[nodiscard]]
  auto stop() -> std::expected<void, MotorError>;

 private:
  const gpio_num_t m_ena_pin;
  const gpio_num_t m_in1_pin;
  const gpio_num_t m_in2_pin;
  const ledc_channel_t m_channel;
  const uint32_t m_frequency;

  static constexpr ledc_timer_bit_t RESOLUTION = LEDC_TIMER_10_BIT;
  static constexpr uint16_t MAX_DUTY = (1 << 10) - 1;  // 1023 for 10-bit
  static constexpr uint16_t MIN_DUTY = 0;

  std::unique_ptr<PwmController> m_pwm_controller;
};

}  // namespace gpio