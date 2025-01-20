#pragma once

#include <array>
#include <atomic>
#include <cstdint>

struct MotorCommand {
  static constexpr int data_size = 5;  // 4 speeds + 1 direction byte
  std::array<uint8_t, 4> speeds;
  uint8_t directions;
  uint64_t sequence;   // For detecting new commands
  uint64_t timestamp;  // For timeout detection

  [[nodiscard]] inline auto getScaledSpeed(uint8_t motorIndex) const -> uint16_t {
    return (uint16_t)speeds[motorIndex] * 4;  // Scale 0-255 to 0-1023
  }

  [[nodiscard]] inline auto getDirection(uint8_t motorIndex) const -> bool {
    return ((directions >> motorIndex) & 0x01) == 0;  // 0=forward, 1=backward
  }
};

// should be 3 motor commands as binary data
auto write_motor_data(const uint8_t* data) -> void;
auto write_motor_data_zero() -> void;

auto motor_control_task(void* arg) -> void;
