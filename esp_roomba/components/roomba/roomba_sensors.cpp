#include "roomba.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static auto delayMs(uint32_t ms) -> void {
  vTaskDelay(pdMS_TO_TICKS(ms));
}


template <size_t N>
auto Roomba::readSensor(SensorPacket packetId) noexcept -> std::expected<std::array<uint8_t, N>, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }

  std::lock_guard<std::mutex> lock(uart_mutex_);

  const uint8_t cmd = CMD_SENSORS;
  const auto id = static_cast<uint8_t>(packetId);

  if (
    uart_write_bytes(config_.uart_num, (const char*)&cmd, 1) < 0 ||
    uart_wait_tx_done(config_.uart_num, pdMS_TO_TICKS(WAIT_TX_MS)) != ESP_OK) {
    return std::unexpected(Error::CommandError);
  }

  delayMs(SENSOR_CMD_GAP_MS);

  if (
    uart_write_bytes(config_.uart_num, (const char*)&id, 1) < 0 ||
    uart_wait_tx_done(config_.uart_num, pdMS_TO_TICKS(WAIT_TX_MS)) != ESP_OK) {
    return std::unexpected(Error::CommandError);
  }

  delayMs(SENSOR_RESPONSE_WAIT_MS);

  std::array<uint8_t, N> response{};
  for (int attempt = 0; attempt < ATTEMPTS_SENSOR_RD; ++attempt) {
    size_t availableBytes = 0;
    if (uart_get_buffered_data_len(config_.uart_num, &availableBytes) != ESP_OK) {
      continue;
    }

    if (availableBytes >= (size_t)N) {
      int readLen = uart_read_bytes(config_.uart_num, response.data(), N, pdMS_TO_TICKS(WAIT_TX_MS));
      if (readLen == (int)N) {
        return response;
      }
    }
    uart_flush(config_.uart_num);
    delayMs(SENSOR_RESPONSE_WAIT_MS);
  }

  return std::unexpected(Error::SensorError);
}

// Explicit template instantiations
template auto Roomba::readSensor<1>(SensorPacket) noexcept -> std::expected<std::array<uint8_t, 1>, Error>;
template auto Roomba::readSensor<2>(SensorPacket) noexcept -> std::expected<std::array<uint8_t, 2>, Error>;
template auto Roomba::readSensor<3>(SensorPacket) noexcept -> std::expected<std::array<uint8_t, 3>, Error>;
template auto Roomba::readSensor<4>(SensorPacket) noexcept -> std::expected<std::array<uint8_t, 4>, Error>;

auto Roomba::dumpAllSensors() noexcept -> std::expected<void, Error> {
  static constexpr size_t ALL_SENSORS_SIZE = 80;
  auto raw = readSensor<ALL_SENSORS_SIZE>(SensorPacket(100));
  if (!raw) {
    return std::unexpected(raw.error());
  }

  const auto& data = *raw;
  int idx = 0;

  // Helpers
  auto readU8 = [&]() -> uint8_t { return data[idx++]; };
  auto readS8 = [&]() -> int8_t { return (int8_t)data[idx++]; };
  auto readU16 = [&]() -> uint16_t {
    uint16_t v = (uint16_t)((data[idx] << 8) | data[idx + 1]);
    idx += 2;
    return v;
  };
  auto readS16 = [&]() -> int16_t {
    int16_t v = (int16_t)((data[idx] << 8) | data[idx + 1]);
    idx += 2;
    return v;
  };

  // Parse subpackets in order (7..58)
  uint8_t bumps = readU8();                    // 7
  uint8_t wall = readU8();                     // 8
  uint8_t cliffLeft = readU8();                // 9
  uint8_t cliffFrontLeft = readU8();           // 10
  uint8_t cliffFrontRight = readU8();          // 11
  uint8_t cliffRight = readU8();               // 12
  uint8_t virtualWall = readU8();              // 13
  uint8_t overcurrents = readU8();             // 14
  uint8_t dirtDetect = readU8();               // 15
  uint8_t unused16 = readU8();                 // 16
  uint8_t irOpCode = readU8();                 // 17
  uint8_t buttons = readU8();                  // 18
  int16_t distance = readS16();                // 19
  int16_t angle = readS16();                   // 20
  uint8_t chargingState = readU8();            // 21
  uint16_t voltage = readU16();                // 22
  int16_t current = readS16();                 // 23
  int8_t temperature = readS8();               // 24
  uint16_t batteryCharge = readU16();          // 25
  uint16_t batteryCapacity = readU16();        // 26
  uint16_t wallSignal = readU16();             // 27
  uint16_t cliffLeftSignal = readU16();        // 28
  uint16_t cliffFrontLeftSignal = readU16();   // 29
  uint16_t cliffFrontRightSignal = readU16();  // 30
  uint16_t cliffRightSignal = readU16();       // 31
  uint16_t unused32 = readU16();               // 32
  uint16_t unused33 = readU16();               // 33
  uint8_t chargerAvailable = readU8();         // 34
  uint8_t oiMode = readU8();                   // 35
  uint8_t songNumber = readU8();               // 36
  uint8_t songPlaying = readU8();              // 37
  uint8_t streamPackets = readU8();            // 38
  int16_t reqVelocity = readS16();             // 39
  int16_t reqRadius = readS16();               // 40
  int16_t reqRightVel = readS16();             // 41
  int16_t reqLeftVel = readS16();              // 42
  uint16_t leftEnc = readU16();                // 43
  uint16_t rightEnc = readU16();               // 44
  uint8_t lightBumper = readU8();              // 45
  uint16_t lbLeft = readU16();                 // 46
  uint16_t lbFrontLeft = readU16();            // 47
  uint16_t lbCenterLeft = readU16();           // 48
  uint16_t lbCenterRight = readU16();          // 49
  uint16_t lbFrontRight = readU16();           // 50
  uint16_t lbRight = readU16();                // 51
  uint8_t irOpCodeLeft = readU8();             // 52
  uint8_t irOpCodeRight = readU8();            // 53
  int16_t leftMotorCurrent = readS16();        // 54
  int16_t rightMotorCurrent = readS16();       // 55
  int16_t mainBrushCurrent = readS16();        // 56
  int16_t sideBrushCurrent = readS16();        // 57
  uint8_t stasis = readU8();                   // 58

  if (idx != (int)ALL_SENSORS_SIZE) {
    ESP_LOGW(LOG_TAG_, "Parsing mismatch? final idx=%d, expected 80", idx);
  }

  // Log all the sensor data
  ESP_LOGI(LOG_TAG_, "==== Full Sensor Dump (Group 100) ====");
  ESP_LOGI(LOG_TAG_, "Bumps/WheelDrops: 0x%02X", bumps);
  ESP_LOGI(LOG_TAG_, "Wall: %u", wall);
  ESP_LOGI(
    LOG_TAG_,
    "CliffLeft: %u, CliffFrontLeft: %u, CliffFrontRight: %u, CliffRight: %u",
    cliffLeft,
    cliffFrontLeft,
    cliffFrontRight,
    cliffRight);
  ESP_LOGI(LOG_TAG_, "VirtualWall: %u", virtualWall);
  ESP_LOGI(LOG_TAG_, "Overcurrents: 0x%02X", overcurrents);
  ESP_LOGI(LOG_TAG_, "DirtDetect: %u", dirtDetect);
  ESP_LOGI(LOG_TAG_, "Unused16: %u", unused16);
  ESP_LOGI(LOG_TAG_, "IR OpCode: %u", irOpCode);
  ESP_LOGI(LOG_TAG_, "Buttons: 0x%02X", buttons);
  ESP_LOGI(LOG_TAG_, "Distance: %d mm, Angle: %d deg", distance, angle);
  ESP_LOGI(
    LOG_TAG_,
    "ChargingState: %u, Voltage: %u mV, Current: %d mA, Temp: %dC",
    chargingState,
    voltage,
    current,
    temperature);
  ESP_LOGI(LOG_TAG_, "BatteryCharge: %u mAh, BatteryCapacity: %u mAh", batteryCharge, batteryCapacity);
  ESP_LOGI(
    LOG_TAG_,
    "WallSignal: %u, CliffLeftSig: %u, CliffFrontLeftSig: %u, CliffFrontRightSig: %u, CliffRightSig: %u",
    wallSignal,
    cliffLeftSignal,
    cliffFrontLeftSignal,
    cliffFrontRightSignal,
    cliffRightSignal);
  ESP_LOGI(LOG_TAG_, "Unused32: %u, Unused33: %u", unused32, unused33);
  ESP_LOGI(LOG_TAG_, "ChargerAvailable: %u, OIMode: %u", chargerAvailable, oiMode);
  ESP_LOGI(LOG_TAG_, "SongNumber: %u, SongPlaying: %u, StreamPackets: %u", songNumber, songPlaying, streamPackets);
  ESP_LOGI(LOG_TAG_, "RequestedVelocity: %d mm/s, RequestedRadius: %d mm", reqVelocity, reqRadius);
  ESP_LOGI(LOG_TAG_, "ReqRightVel: %d, ReqLeftVel: %d", reqRightVel, reqLeftVel);
  ESP_LOGI(LOG_TAG_, "LeftEnc: %u, RightEnc: %u", leftEnc, rightEnc);
  ESP_LOGI(LOG_TAG_, "LightBumper: 0x%02X", lightBumper);
  ESP_LOGI(
    LOG_TAG_,
    "LightBump(Left,FrontLeft,CenterLeft,CenterRight,FrontRight,Right)= %u,%u,%u,%u,%u,%u",
    lbLeft,
    lbFrontLeft,
    lbCenterLeft,
    lbCenterRight,
    lbFrontRight,
    lbRight);
  ESP_LOGI(LOG_TAG_, "IR(Left,Right)= %u,%u", irOpCodeLeft, irOpCodeRight);
  ESP_LOGI(
    LOG_TAG_,
    "MotorCurrent(Left,Right,MainBrush,SideBrush)= %d,%d,%d,%d",
    leftMotorCurrent,
    rightMotorCurrent,
    mainBrushCurrent,
    sideBrushCurrent);
  ESP_LOGI(LOG_TAG_, "Stasis: %u", stasis);

  return {};
} 