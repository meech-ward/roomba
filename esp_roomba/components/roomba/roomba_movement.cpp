#include "roomba.hpp"

#include <algorithm>
#include "esp_log.h"

auto Roomba::drive(int16_t velocity, int16_t radius) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (currentMode_ == Mode::Off || currentMode_ == Mode::Passive) {
    return std::unexpected(Error::InvalidParameter);
  }

  velocity = std::clamp<int16_t>(velocity, -MAX_DRIVE_SPEED, MAX_DRIVE_SPEED);

  // clamp radius only if not special 32767 or -1
  if (radius != 32767 && radius != -1) {
    radius = std::clamp<int16_t>(radius, -MAX_DRIVE_RADIUS, MAX_DRIVE_RADIUS);
  }

  uint8_t data[4]{
    (uint8_t)((velocity >> 8) & 0xFF),
    (uint8_t)(velocity & 0xFF),
    (uint8_t)((radius >> 8) & 0xFF),
    (uint8_t)(radius & 0xFF)};

  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_DRIVE, data);
}

auto Roomba::driveDirect(int16_t rightVelocity, int16_t leftVelocity) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (currentMode_ == Mode::Off || currentMode_ == Mode::Passive) {
    return std::unexpected(Error::InvalidParameter);
  }

  rightVelocity = std::clamp<int16_t>(rightVelocity, -MAX_DRIVE_SPEED, MAX_DRIVE_SPEED);
  leftVelocity = std::clamp<int16_t>(leftVelocity, -MAX_DRIVE_SPEED, MAX_DRIVE_SPEED);

  uint8_t data[4]{
    (uint8_t)((rightVelocity >> 8) & 0xFF),
    (uint8_t)(rightVelocity & 0xFF),
    (uint8_t)((leftVelocity >> 8) & 0xFF),
    (uint8_t)(leftVelocity & 0xFF)};

  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_DRIVE_DIRECT, data);
}

auto Roomba::driveStop() noexcept -> std::expected<void, Error> {
  return drive(0, 0);
}

auto Roomba::drivePWM(int16_t rightPWM, int16_t leftPWM) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (currentMode_ == Mode::Off || currentMode_ == Mode::Passive) {
    return std::unexpected(Error::InvalidParameter);
  }

  rightPWM = std::clamp<int16_t>(rightPWM, -255, 255);
  leftPWM = std::clamp<int16_t>(leftPWM, -255, 255);

  uint8_t data[4]{
    static_cast<uint8_t>((rightPWM >> 8) & 0xFF),
    static_cast<uint8_t>(rightPWM & 0xFF),
    static_cast<uint8_t>((leftPWM >> 8) & 0xFF),
    static_cast<uint8_t>(leftPWM & 0xFF)
  };

  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_DRIVE_PWM, data);
} 