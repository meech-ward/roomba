#include "pwm_controller.hpp"

namespace gpio {

auto PwmController::init(const std::vector<PwmChannelConfig>& channels) -> std::expected<void, PwmError> {
  if (channels.empty()) {
    return std::unexpected(PwmError::InvalidParameter);
  }

  m_channels = channels;

  for (auto& cfg : m_channels) {
    // Configure the timer first
    auto timerResult = configureTimer(cfg);
    if (!timerResult) {
      return std::unexpected(timerResult.error());
    }

    // Configure the channel
    auto chanResult = configureChannel(cfg);
    if (!chanResult) {
      return std::unexpected(chanResult.error());
    }
  }

  // Success
  return {};
}

auto PwmController::setDutyCycle(std::size_t channelIndex, uint32_t newDuty) -> std::expected<void, PwmError> {
  if (channelIndex >= m_channels.size()) {
    return std::unexpected(PwmError::InvalidParameter);
  }

  auto& cfg = m_channels[channelIndex];
  cfg.duty = newDuty;

  // First set the duty
  esp_err_t err = ledc_set_duty(cfg.speedMode, cfg.channel, newDuty);
  if (err != ESP_OK) {
    return std::unexpected(PwmError::DutySetFailed);
  }

  // Then update the duty in hardware
  err = ledc_update_duty(cfg.speedMode, cfg.channel);
  if (err != ESP_OK) {
    return std::unexpected(PwmError::DutySetFailed);
  }

  return {};
}

auto PwmController::setFrequency(std::size_t channelIndex, uint32_t newFrequency) -> std::expected<void, PwmError> {
  if (channelIndex >= m_channels.size() || newFrequency == 0) {
    return std::unexpected(PwmError::InvalidParameter);
  }

  auto& cfg = m_channels[channelIndex];
  cfg.frequency = newFrequency;

  // Reconfigure the timer with the new frequency.
  // Note that any channel that uses the same (speedMode, timer) is also affected.
  ledc_timer_config_t timerConfig = {
    .speed_mode = cfg.speedMode,
    .duty_resolution = cfg.resolution,
    .timer_num = cfg.timer,
    .freq_hz = newFrequency,
    .clk_cfg = LEDC_AUTO_CLK,
    .deconfigure = false
    };

  esp_err_t err = ledc_timer_config(&timerConfig);
  if (err != ESP_OK) {
    return std::unexpected(PwmError::FrequencySetFailed);
  }

  return {};
}

auto PwmController::configureTimer(const PwmChannelConfig& channelCfg) -> std::expected<void, PwmError> {
  ledc_timer_config_t timerConfig = {
    .speed_mode = channelCfg.speedMode,  // Must be LEDC_LOW_SPEED_MODE on ESP32-S3
    .duty_resolution = channelCfg.resolution,
    .timer_num = channelCfg.timer,
    .freq_hz = channelCfg.frequency,
    .clk_cfg = LEDC_AUTO_CLK,
    .deconfigure = false};

  esp_err_t err = ledc_timer_config(&timerConfig);
  if (err != ESP_OK) {
    return std::unexpected(PwmError::TimerConfigFailed);
  }

  return {};
}

auto PwmController::configureChannel(const PwmChannelConfig& channelCfg) -> std::expected<void, PwmError> {
  ledc_channel_config_t ledcChannel = {
    .gpio_num = channelCfg.gpioPin,
    .speed_mode = channelCfg.speedMode,  // Must be LEDC_LOW_SPEED_MODE
    .channel = channelCfg.channel,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = channelCfg.timer,
    .duty = channelCfg.duty,
    .hpoint = 0,
    .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
    .flags = {0}
  };

  esp_err_t err = ledc_channel_config(&ledcChannel);
  if (err != ESP_OK) {
    return std::unexpected(PwmError::ChannelConfigFailed);
  }

  return {};
}

}  // namespace gpio