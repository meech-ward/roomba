#pragma once

#include <array>
#include <atomic>
#include <cstdint>

static constexpr int8_t MIN_SPEED = -100;
static constexpr int8_t MAX_SPEED = 100;

struct MotorCommand {
  static constexpr int data_size = 4;
  std::array<int8_t, 4> speeds;
  uint64_t sequence;
  uint64_t timestamp;

  [[nodiscard]] inline auto getScaledSpeed(uint8_t motorIndex) const -> int16_t {
    // Convert -100 to 100 range to -1023 to 1023
    // First clamp to ensure we're in range
    int8_t clampedSpeed = std::max(MIN_SPEED, std::min(MAX_SPEED, speeds[motorIndex]));

    // Scale the value: (speed * 1023) / 100
    return (static_cast<int16_t>(clampedSpeed) * 1023) / 100;
  }

  [[nodiscard]] inline auto getDirection(uint8_t motorIndex) const -> bool {
    return speeds[motorIndex] >= 0;  // true = forward (positive), false = backward (negative)
  }

  [[nodiscard]] inline auto getAbsoluteSpeed(uint8_t motorIndex) const -> uint16_t {
    return static_cast<uint16_t>(std::abs(getScaledSpeed(motorIndex)));
  }
};

// Functions remain the same but now expect 4 bytes of signed data
auto write_motor_data(const uint8_t* data) -> void;
auto write_motor_data_zero() -> void;

auto motor_control_task(void* arg) -> void;