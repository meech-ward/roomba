#include "motor.hpp"

#include <freertos/FreeRTOS.h>

#include "hal/gpio_types.h"

namespace gpio {

Motor::Motor(gpio_num_t ena_pin, gpio_num_t in1_pin, gpio_num_t in2_pin, ledc_channel_t channel, uint32_t frequency)
    : m_ena_pin(ena_pin),
      m_in1_pin(in1_pin),
      m_in2_pin(in2_pin),
      m_channel(channel),
      m_frequency(frequency),
      m_pwm_controller(std::make_unique<PwmController>()) {}

auto Motor::init() -> std::expected<void, MotorError> {
  // Configure direction control pins

  gpio_reset_pin(m_in1_pin);  // Clear any UART config
  gpio_reset_pin(m_in2_pin);

  vTaskDelay(pdMS_TO_TICKS(1));

  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << m_in1_pin) | (1ULL << m_in2_pin);
  io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

  if (gpio_config(&io_conf) != ESP_OK) {
    return std::unexpected(MotorError::GpioInitFailed);
  }

  // Configure PWM channel for motor
  PwmChannelConfig pwm_config{
    .speedMode = LEDC_LOW_SPEED_MODE,
    .timer = LEDC_TIMER_1,  // Use timer 1 for motors
    .channel = m_channel,   // First channel
    .gpioPin = static_cast<int>(m_ena_pin),
    .resolution = RESOLUTION,
    .frequency = m_frequency,
    .duty = 0  // Start with motor stopped
  };

  // Initialize PWM controller with single channel
  auto result = m_pwm_controller->init({pwm_config});
  if (!result) {
    return std::unexpected(MotorError::PwmInitFailed);
  }

  return {};
}

auto Motor::forward(uint16_t speed) -> std::expected<void, MotorError> {
  // Clamp speed to 0-100%
  speed = std::clamp(speed, MIN_DUTY, MAX_DUTY);

  // Set direction pins
  gpio_set_level(m_in1_pin, 1);
  gpio_set_level(m_in2_pin, 0);

  auto result = m_pwm_controller->setDutyCycle(0, speed);
  if (!result) {
    return std::unexpected(MotorError::PwmSetFailed);
  }

  return {};
}

auto Motor::backward(uint16_t speed) -> std::expected<void, MotorError> {
  // Clamp speed to 0-100%
  speed = std::clamp(speed, MIN_DUTY, MAX_DUTY);

  // Set direction pins
  gpio_set_level(m_in1_pin, 0);
  gpio_set_level(m_in2_pin, 1);

  auto result = m_pwm_controller->setDutyCycle(0, speed);
  if (!result) {
    return std::unexpected(MotorError::PwmSetFailed);
  }

  return {};
}

auto Motor::stop() -> std::expected<void, MotorError> {
  // Set both direction pins low
  gpio_set_level(m_in1_pin, 0);
  gpio_set_level(m_in2_pin, 0);

  // Set PWM duty to 0
  auto result = m_pwm_controller->setDutyCycle(0, 0);
  if (!result) {
    return std::unexpected(MotorError::PwmSetFailed);
  }

  return {};
}
}  // namespace gpio