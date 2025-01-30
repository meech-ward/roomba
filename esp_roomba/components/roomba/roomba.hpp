#pragma once

#include <array>
#include <cstdint>
#include <expected>  // or a 3rd-party "tl::expected" if your C++ lib doesn't have <expected>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>
#include <functional>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

class Roomba {
 public:
  // ------------------------- Error Types -------------------------
  enum class Error { UartError, InitializationError, CommandError, SensorError, InvalidParameter, Timeout };

  // ------------------------- Operating Modes -------------------------
  enum class Mode { Off, Passive, Safe, Full };

  // ------------------------- Motors and LEDs -------------------------
  enum class Motor : uint8_t { MainBrush = 0x04, Vacuum = 0x02, SideBrush = 0x01 };

  enum class Led : uint8_t { Debris = 0x01, Spot = 0x02, Dock = 0x04, Check = 0x08 };

  // ------------------------- Sensor Packets -------------------------
  enum class SensorPacket : uint8_t {
    Bumps = 7,
    WallSensor = 8,  // 1 is left, 2 is right. the big bumper thing
    CliffLeft = 9,
    CliffFrontLeft = 10,
    CliffFrontRight = 11,
    CliffRight = 12,
    VirtualWall = 13,
    WheelOvercurrents = 14,
    DirtDetect = 15,
    InfraredCharacter = 17,
    Buttons = 18,
    Distance = 19,
    Angle = 20,
    ChargingState = 21,
    Voltage = 22,
    Current = 23,
    Temperature = 24,
    BatteryCharge = 25,
    BatteryCapacity = 26,
    WallSignal = 27,
    CliffLeftSignal = 28,
    CliffFrontLeftSignal = 29,
    CliffFrontRightSignal = 30,
    CliffRightSignal = 31,
    ChargingSourcesAvailable = 34,
    OIMode = 35,
    SongNumber = 36,
    SongPlaying = 37,
    StreamPackets = 38,
    RequestedVelocity = 39,
    RequestedRadius = 40,
    RequestedRightVelocity = 41,
    RequestedLeftVelocity = 42,
    LeftEncoderCounts = 43,
    RightEncoderCounts = 44,
    LightBumper = 45,
    LightBumpLeft = 46,
    LightBumpFrontLeft = 47,
    LightBumpCenterLeft = 48,
    LightBumpCenterRight = 49,
    LightBumpFrontRight = 50,
    LightBumpRight = 51,
    LeftMotorCurrent = 54,
    RightMotorCurrent = 55,
    MainBrushMotorCurrent = 56,
    SideBrushMotorCurrent = 57,
    Stasis = 58
  };

  // ------------------------- Configuration -------------------------
  struct Config {
    uart_port_t uart_num;  // e.g. UART_NUM_1
    gpio_num_t tx_pin;     // e.g. GPIO_NUM_6
    gpio_num_t rx_pin;     // e.g. GPIO_NUM_7
    gpio_num_t brc_pin;    // e.g. GPIO_NUM_8
    uint32_t baud_rate;    // e.g. 115200
    bool use_brc;          // true to use BRC for waking

    Config();  // defined in .cpp
  };
  // TX (GPIO 6) → Pin 3 (RXD)
  // RX (GPIO 7) → Pin 4 (TXD)
  // BRC (GPIO 8) → Pin 5 (BRC)
  // Ground → Pins 6/7 (GND)

  // ------------------------- Constructors / Destructors -------------------------
  explicit Roomba(const Config& cfg);
  Roomba(Roomba const&) = delete;
  auto operator=(Roomba const&) -> Roomba& = delete;

  Roomba(Roomba&& other) noexcept;
  auto operator=(Roomba&& other) noexcept -> Roomba&;

  ~Roomba();

  // ------------------------- Core Functionality -------------------------
  [[nodiscard]] auto wake() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto start() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto reset() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto stop() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto power() noexcept -> std::expected<void, Error>;

  // ------------------------- Mode Control -------------------------
  [[nodiscard]] auto setSafeMode() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto setFullMode() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto getMode() noexcept -> std::expected<Mode, Error>;

  // ------------------------- Movement Commands -------------------------
  [[nodiscard]] auto drive(int16_t velocity, int16_t radius) noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto driveDirect(int16_t rightVelocity, int16_t leftVelocity) noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto driveStop() noexcept -> std::expected<void, Error>;

  // ------------------------- Cleaning Commands -------------------------
  [[nodiscard]] auto clean() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto spot() noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto dock() noexcept -> std::expected<void, Error>;

  // ------------------------- Motor Control -------------------------
  [[nodiscard]] auto setMotors(uint8_t mainBrush, uint8_t sideBrush, uint8_t vacuum) noexcept
    -> std::expected<void, Error>;

  // ------------------------- LED Control -------------------------
  [[nodiscard]] auto setLeds(uint8_t leds, uint8_t powerColor, uint8_t powerIntensity) noexcept
    -> std::expected<void, Error>;

  // ------------------------- Music Control -------------------------
  [[nodiscard]] auto song(
    uint8_t songNumber,
    std::span<const uint8_t> notes,
    std::span<const uint8_t> durations) noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto playSong(uint8_t songNumber) noexcept -> std::expected<void, Error>;

  // Play a "crowd pleaser" snippet
  [[nodiscard]] auto playCrowdPleaserSong() noexcept -> std::expected<void, Error>;
  [[nodiscard]] auto playCrowdPleaserSong2() noexcept -> std::expected<void, Error>;
  [[nodiscard]] auto playDaftPunkSong() noexcept -> std::expected<void, Error>;
  [[nodiscard]] auto playInTheEnd() noexcept -> std::expected<void, Error>;
  // ------------------------- Sensor Reading (templated) -------------------------
  template <size_t N>
  [[nodiscard]] auto readSensor(SensorPacket packetId) noexcept -> std::expected<std::array<uint8_t, N>, Error>;

  [[nodiscard]] auto dumpAllSensors() noexcept -> std::expected<void, Error>;

  // ------------------------- Utility -------------------------
  [[nodiscard]] auto ChangeBaudRate(uint32_t baudRate) noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto isInitialized() const noexcept -> bool {
    return initialized_;
  }

  /**
   * @brief Write up to 4 ASCII characters to the 4-digit display
   * @param text String to display (up to 4 characters)
   * @return Expected void or Error
   * @note Only certain ASCII characters are supported (32-126)
   *       Display will show approximations for some characters
   */
  auto writeToDisplay(std::string_view text) noexcept
    -> std::expected<void, Error>;

  /**
   * @brief Control wheels directly with PWM values
   * @param rightPWM PWM value for right wheel (-255 to 255)
   *                 Positive values move the wheel forward
   *                 Negative values move the wheel backward
   * @param leftPWM PWM value for left wheel (-255 to 255)
   *                Positive values move the wheel forward
   *                Negative values move the wheel backward
   * @return Expected void or Error
   * @note Requires Safe or Full mode
   */
  [[nodiscard]] auto drivePWM(int16_t rightPWM, int16_t leftPWM) noexcept 
    -> std::expected<void, Error>;

  /**
   * @brief Start streaming the specified sensor packets
   * @param packets List of sensor packet IDs to stream
   * @return Expected void or Error
   */
  [[nodiscard]] auto startStreaming(std::span<const SensorPacket> packets) noexcept 
    -> std::expected<void, Error>;

  /**
   * @brief Stop the sensor stream
   * @return Expected void or Error
   */
  [[nodiscard]] auto stopStreaming() noexcept -> std::expected<void, Error>;

  /**
   * @brief Read the next packet from the sensor stream
   * @return Expected vector of bytes containing the stream data, or Error
   */
  [[nodiscard]] auto readStream() noexcept -> std::expected<std::vector<uint8_t>, Error>;

  // Add these new types
  struct StreamPacket {
    std::vector<uint8_t> data;
    uint32_t timestamp;  // ESP.getTime() timestamp
  };

  using StreamCallback = std::function<void(const StreamPacket&)>;

  // Add these new methods
  [[nodiscard]] auto startStreamTask(StreamCallback callback) noexcept -> std::expected<void, Error>;
  [[nodiscard]] auto stopStreamTask() noexcept -> std::expected<void, Error>;

 private:
  // ---------------------------------------------------------------------
  // Constants / Opcodes
  // ---------------------------------------------------------------------
  static constexpr const char* LOG_TAG_ = "Roomba";

  static constexpr uint8_t CMD_START = 128;
  static constexpr uint8_t CMD_BAUD = 129;
  static constexpr uint8_t CMD_SAFE = 131;
  static constexpr uint8_t CMD_FULL = 132;
  static constexpr uint8_t CMD_CLEAN = 135;
  static constexpr uint8_t CMD_SPOT = 134;
  static constexpr uint8_t CMD_DOCK = 143;
  static constexpr uint8_t CMD_POWER = 133;
  static constexpr uint8_t CMD_DRIVE = 137;
  static constexpr uint8_t CMD_DRIVE_DIRECT = 145;
  static constexpr uint8_t CMD_MOTORS = 138;
  static constexpr uint8_t CMD_LEDS = 139;
  static constexpr uint8_t CMD_SONG = 140;
  static constexpr uint8_t CMD_PLAY = 141;
  static constexpr uint8_t CMD_RESET = 7;
  static constexpr uint8_t CMD_STOP = 173;
  static constexpr uint8_t CMD_SENSORS = 142;
  static constexpr uint8_t CMD_STREAM_SENSORS = 148;
  static constexpr uint8_t CMD_DIGITS = 164;  // Command for 4-digit display
  static constexpr uint8_t CMD_DRIVE_PWM = 146;  // Add with other command constants
  static constexpr uint8_t CMD_PAUSE_RESUME_STREAM = 150;
  static constexpr uint8_t STREAM_HEADER = 19;
  static constexpr int STREAM_READ_TIMEOUT_MS = 100;

  // Some standard limits from the Roomba OI spec
  static constexpr int16_t MAX_DRIVE_SPEED = 500;
  static constexpr int16_t MAX_DRIVE_RADIUS = 2000;
  static constexpr int ATTEMPTS_SENSOR_RD = 3;
  static constexpr int RX_BUFFER_SIZE = 256;
  static constexpr int TX_BUFFER_SIZE = 256;

  // Common wait times (ms)
  static constexpr int WAIT_TX_MS = 100;
  static constexpr int WAKE_HIGH_MS = 100;
  static constexpr int WAKE_LOW_MS = 500;
  static constexpr int MODE_SETTLE_MS = 100;
  static constexpr int POST_RESET_DELAY_MS = 1000;
  static constexpr int SENSOR_CMD_GAP_MS = 20;
  static constexpr int SENSOR_RESPONSE_WAIT_MS = 50;

  // Private data
  bool initialized_{false};
  Mode currentMode_{Mode::Off};
  Config config_{};

  // A mutex to guard UART usage (thread-safety)
  mutable std::mutex uart_mutex_;

  // Private init
  auto init() noexcept -> void;

  // Private send
  [[nodiscard]] auto sendCommand(uint8_t cmd) noexcept -> std::expected<void, Error>;

  [[nodiscard]] auto sendCommand(uint8_t cmd, std::span<const uint8_t> data) noexcept -> std::expected<void, Error>;

  // STREAMING
  TaskHandle_t stream_task_handle_{nullptr};
  StreamCallback stream_callback_{nullptr};
  bool stream_task_running_{false};
  
  // Private streaming task method
  static void streamTaskFunction(void* arg);
};