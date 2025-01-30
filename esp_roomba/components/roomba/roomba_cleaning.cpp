#include "roomba.hpp"

auto Roomba::clean() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_CLEAN);
}

auto Roomba::spot() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_SPOT);
}

auto Roomba::dock() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_DOCK);
}

auto Roomba::setMotors(uint8_t mainBrush, uint8_t sideBrush, uint8_t vacuum) noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (currentMode_ == Mode::Off || currentMode_ == Mode::Passive) {
    return std::unexpected(Error::InvalidParameter);
  }

  uint8_t motors = 0;
  if (mainBrush)
    motors |= (uint8_t)Motor::MainBrush;
  if (sideBrush)
    motors |= (uint8_t)Motor::SideBrush;
  if (vacuum)
    motors |= (uint8_t)Motor::Vacuum;

  std::lock_guard<std::mutex> lock(uart_mutex_);
  return sendCommand(CMD_MOTORS, std::span(&motors, 1));
} 