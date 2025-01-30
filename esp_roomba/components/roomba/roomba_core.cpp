#include "roomba.hpp"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 57600 for older models, 115200 for newer models
static constexpr uint32_t ROOMBA_BAUD_RATE = 115200;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility: convert milliseconds to FreeRTOS ticks
//////////////////////////////////////////////////////////////////////////////////////////////////////
static auto delayMs(uint32_t ms) -> void {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

// ----------------------------------------------------------------------------
//  Roomba::Config default constructor
// ----------------------------------------------------------------------------
Roomba::Config::Config()
    : uart_num(UART_NUM_1),
      tx_pin(GPIO_NUM_6),
      rx_pin(GPIO_NUM_7),
      brc_pin(GPIO_NUM_8),
      baud_rate(ROOMBA_BAUD_RATE),
      use_brc(true) {
  // empty
}

// ----------------------------------------------------------------------------
//  Roomba constructor / destructor
// ----------------------------------------------------------------------------
Roomba::Roomba(const Config& cfg) : config_(cfg) {
  init();
}

Roomba::~Roomba() {
  if (initialized_) {
    // Attempt to stop
    (void)stop();

    // Delete UART driver
    uart_driver_delete(config_.uart_num);

    // Reset BRC if used
    if (config_.use_brc) {
      gpio_reset_pin(config_.brc_pin);
    }
  }
}

Roomba::Roomba(Roomba&& other) noexcept {
  // We have to lock the other's mutex
  std::lock_guard<std::mutex> lock(other.uart_mutex_);
  config_ = other.config_;
  initialized_ = other.initialized_;
  currentMode_ = other.currentMode_;

  // Invalidate the other
  other.initialized_ = false;
  other.currentMode_ = Mode::Off;
}

auto Roomba::operator=(Roomba&& other) noexcept -> Roomba& {
  if (this != &other) {
    // Lock both
    std::lock(uart_mutex_, other.uart_mutex_);
    std::lock_guard<std::mutex> lhsLock(uart_mutex_, std::adopt_lock);
    std::lock_guard<std::mutex> rhsLock(other.uart_mutex_, std::adopt_lock);

    if (initialized_) {
      (void)stop();
      uart_driver_delete(config_.uart_num);
      if (config_.use_brc) {
        gpio_reset_pin(config_.brc_pin);
      }
    }

    config_ = other.config_;
    initialized_ = other.initialized_;
    currentMode_ = other.currentMode_;

    other.initialized_ = false;
    other.currentMode_ = Mode::Off;
  }
  return *this;
}

// ----------------------------------------------------------------------------
//  Private init()
// ----------------------------------------------------------------------------
auto Roomba::init() noexcept -> void {
  if (config_.use_brc) {
    gpio_config_t io_conf{
      .pin_bit_mask = (1ULL << config_.brc_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE};
    if (auto rc = gpio_config(&io_conf); rc != ESP_OK) {
      ESP_LOGE(LOG_TAG_, "Failed to config BRC pin: %s", esp_err_to_name(rc));
      return;
    }
  }

  // UART config
  uart_config_t uart_conf{
    .baud_rate = (int)config_.baud_rate,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT};

  if (auto rc = uart_param_config(config_.uart_num, &uart_conf); rc != ESP_OK) {
    ESP_LOGE(LOG_TAG_, "uart_param_config fail: %s", esp_err_to_name(rc));
    return;
  }

  if (auto rc = uart_set_pin(config_.uart_num, config_.tx_pin, config_.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
      rc != ESP_OK) {
    ESP_LOGE(LOG_TAG_, "uart_set_pin fail: %s", esp_err_to_name(rc));
    return;
  }

  if (auto rc = uart_driver_install(config_.uart_num, RX_BUFFER_SIZE, TX_BUFFER_SIZE, 0, nullptr, 0); rc != ESP_OK) {
    ESP_LOGE(LOG_TAG_, "uart_driver_install fail: %s", esp_err_to_name(rc));
    return;
  }

  initialized_ = true;
  ESP_LOGI(LOG_TAG_, "Roomba driver initialized.");
}

// ----------------------------------------------------------------------------
//  Basic operations
// ----------------------------------------------------------------------------
auto Roomba::wake() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (config_.use_brc) {
    std::lock_guard<std::mutex> lock(uart_mutex_);
    // Pulse BRC
    gpio_set_level(config_.brc_pin, 1);
    delayMs(WAKE_HIGH_MS);
    gpio_set_level(config_.brc_pin, 0);
    delayMs(WAKE_LOW_MS);
  }
  return {};
}

auto Roomba::start() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);
  auto r = sendCommand(CMD_START);
  if (!r) {
    return r;
  }
  currentMode_ = Mode::Passive;
  delayMs(MODE_SETTLE_MS);
  return {};
}

auto Roomba::reset() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  {
    std::lock_guard<std::mutex> lock(uart_mutex_);
    auto r = sendCommand(CMD_RESET);
    if (!r) {
      return r;
    }
  }
  currentMode_ = Mode::Off;
  delayMs(POST_RESET_DELAY_MS);

  // Then we can do start() again if we want to keep controlling
  return start();
}

auto Roomba::stop() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);

  (void)driveStop();  // attempt to stop motion

  auto r = sendCommand(CMD_STOP);
  if (!r) {
    return r;
  }
  currentMode_ = Mode::Off;
  return {};
}

auto Roomba::power() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  std::lock_guard<std::mutex> lock(uart_mutex_);
  auto r = sendCommand(CMD_POWER);
  if (!r) {
    return r;
  }
  currentMode_ = Mode::Passive;
  return {};
}

auto Roomba::setSafeMode() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (currentMode_ == Mode::Off) {
    return std::unexpected(Error::InvalidParameter);
  }

  std::lock_guard<std::mutex> lock(uart_mutex_);
  auto r = sendCommand(CMD_SAFE);
  if (!r) {
    return r;
  }
  currentMode_ = Mode::Safe;
  delayMs(MODE_SETTLE_MS);
  return {};
}

auto Roomba::setFullMode() noexcept -> std::expected<void, Error> {
  if (!initialized_) {
    return std::unexpected(Error::InitializationError);
  }
  if (currentMode_ == Mode::Off) {
    return std::unexpected(Error::InvalidParameter);
  }

  std::lock_guard<std::mutex> lock(uart_mutex_);
  auto r = sendCommand(CMD_FULL);
  if (!r) {
    return r;
  }
  currentMode_ = Mode::Full;
  delayMs(MODE_SETTLE_MS);
  return {};
}

auto Roomba::getMode() noexcept -> std::expected<Mode, Error> {
  return currentMode_;
}

// ----------------------------------------------------------------------------
//  Private send helpers
// ----------------------------------------------------------------------------
auto Roomba::sendCommand(uint8_t cmd) noexcept -> std::expected<void, Error> {
  int w = uart_write_bytes(config_.uart_num, (const char*)&cmd, 1);
  if (w < 0) {
    return std::unexpected(Error::CommandError);
  }
  return {};
}

auto Roomba::sendCommand(uint8_t cmd, std::span<const uint8_t> data) noexcept -> std::expected<void, Error> {
  int w1 = uart_write_bytes(config_.uart_num, (const char*)&cmd, 1);
  if (w1 < 0) {
    return std::unexpected(Error::CommandError);
  }

  if (!data.empty()) {
    int w2 = uart_write_bytes(config_.uart_num, reinterpret_cast<const char*>(data.data()), data.size());
    if (w2 < 0) {
      return std::unexpected(Error::CommandError);
    }
  }
  ESP_LOGI(LOG_TAG_, "Command sent: %d", cmd);
  return {};
} 