#pragma once

#include <cstdint>
#include <expected>
#include <vector>

#include "driver/ledc.h"

namespace gpio {
/**
 * @brief Error codes for PwmController.
 */
enum class PwmError {
  InvalidParameter,
  TimerConfigFailed,
  ChannelConfigFailed,
  DutySetFailed,
  FrequencySetFailed,
  UnknownError
};

/**
 * @brief Configuration data for a single PWM channel.
 *
 * @note For the ESP32-S3 on IDF 5.4:
 *       - Use only LEDC_LOW_SPEED_MODE for speedMode.
 */
struct PwmChannelConfig {
  ledc_mode_t speedMode;        // Must be LEDC_LOW_SPEED_MODE on ESP32-S3
  ledc_timer_t timer;           // e.g., LEDC_TIMER_0, LEDC_TIMER_1
  ledc_channel_t channel;       // e.g., LEDC_CHANNEL_0..7
  int gpioPin;                  // GPIO number to drive with PWM
  ledc_timer_bit_t resolution;  // e.g., LEDC_TIMER_10_BIT, LEDC_TIMER_13_BIT
  uint32_t frequency;           // in Hz
  uint32_t duty;                // initial duty (0 .. 2^resolution - 1)
};

/**
 * @brief A class to control single or multiple PWM channels on an ESP32-S3 using LEDC.
 *
 * Uses std::expected to return success or an error instead of throwing exceptions.
 */
class PwmController {
 public:
  /**
   * @brief Default constructor.
   */
  PwmController() = default;

  /**
   * @brief Destructor. Optionally de-initializes or stops channels if desired.
   */
  ~PwmController() = default;

  /**
   * @brief Initialize one or more PWM channels.
   *
   * @param channels A vector of channel configurations (pins, freq, resolution, etc.).
   * @return         std::expected<void, PwmError> which contains success or an error.
   */
  [[nodiscard]]
  auto init(const std::vector<PwmChannelConfig>& channels) -> std::expected<void, PwmError>;

  /**
   * @brief Set the duty cycle of an already-initialized channel.
   *
   * @param channelIndex Index in the internally stored vector of channels.
   * @param newDuty      New duty cycle (0 <= duty <= 2^resolution - 1).
   * @return             std::expected<void, PwmError> which contains success or an error.
   */
  [[nodiscard]]
  auto setDutyCycle(std::size_t channelIndex, uint32_t newDuty) -> std::expected<void, PwmError>;

  /**
   * @brief Set the frequency of an already-initialized channel.
   *
   * @param channelIndex Index in the internally stored vector of channels.
   * @param newFrequency New frequency in Hz.
   * @return             std::expected<void, PwmError> which contains success or an error.
   */
  [[nodiscard]]
  auto setFrequency(std::size_t channelIndex, uint32_t newFrequency) -> std::expected<void, PwmError>;

 private:
  // Store configurations for each channel
  std::vector<PwmChannelConfig> m_channels;

  // Helper functions to configure timer/channel
  auto configureTimer(const PwmChannelConfig& channelCfg) -> std::expected<void, PwmError>;
  auto configureChannel(const PwmChannelConfig& channelCfg) -> std::expected<void, PwmError>;
};

}  // namespace gpio