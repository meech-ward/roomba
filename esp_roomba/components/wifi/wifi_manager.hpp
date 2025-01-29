// wifi_manager.hpp
#pragma once

#include <atomic>
#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

namespace wifi {

using namespace std::chrono_literals;

// Forward declarations
class WifiManager;

enum class WifiError {
  None,
  NotInitialized,
  AlreadyInitialized,
  ConnectionFailed,
  AuthenticationFailed,
  NetworkNotFound,
  Timeout,
  SystemError,
  InvalidConfiguration,
  InvalidCredentials,
  WeakSignal
};

struct ConnectionInfo {
  esp_netif_ip_info_t ip_info{};
  int8_t rssi{};
  wifi_auth_mode_t auth_mode{};
  std::string ssid{};
  bool connected{false};
};

struct WifiConfig {
  std::string ssid;
  std::string password;
  std::chrono::milliseconds connection_timeout{10000ms};
  std::chrono::milliseconds retry_interval{1000ms};
  uint8_t max_retries{3};
  wifi_scan_method_t scan_method{WIFI_FAST_SCAN};
  int8_t min_rssi{-127};
  wifi_auth_mode_t min_authmode{WIFI_AUTH_OPEN};
  wifi_ps_type_t power_save{WIFI_PS_MIN_MODEM};
};

using ConnectedCallback = std::function<void(const ConnectionInfo&)>;
using DisconnectedCallback = std::function<void(WifiError)>;
using ConnectionFailedCallback = std::function<void(WifiError)>;

class WifiManager {
 public:
  static auto instance() -> WifiManager&;

  std::atomic<bool> wifi_ready{false};

  // Delete copy and move operations
  WifiManager(const WifiManager&) = delete;
  auto operator=(const WifiManager&) -> WifiManager& = delete;
  WifiManager(WifiManager&&) = delete;
  auto operator=(WifiManager&&) -> WifiManager& = delete;

  [[nodiscard]] auto initialize(const WifiConfig& config = {}) -> std::expected<void, WifiError>;
  [[nodiscard]] auto connect() -> std::expected<void, WifiError>;
  [[nodiscard]] auto disconnect() -> std::expected<void, WifiError>;
  [[nodiscard]] auto get_connection_info() const -> std::expected<ConnectionInfo, WifiError>;
  [[nodiscard]] auto is_connected() const noexcept -> bool {
    return m_connected;
  }

  void on_connected(ConnectedCallback callback) {
    m_connected_callback = std::move(callback);
  }
  void on_disconnected(DisconnectedCallback callback) {
    m_disconnected_callback = std::move(callback);
  }
  void on_connection_failed(ConnectionFailedCallback callback) {
    m_connection_failed_callback = std::move(callback);
  }

  auto initialize_for_scan() -> std::expected<void, WifiError>;
  auto scan_networks() -> std::expected<void, WifiError>;

  class ConnectionGuard {
   public:
    explicit ConnectionGuard(WifiManager& manager);
    ~ConnectionGuard();
    [[nodiscard]] auto success() const -> bool {
      return m_result.has_value();
    }
    [[nodiscard]] auto error() const -> WifiError {
      return m_result.has_value() ? WifiError::None : m_result.error();
    }

   private:
    WifiManager& m_manager;
    std::expected<void, WifiError> m_result;
  };

  [[nodiscard]] auto guard() -> ConnectionGuard {
    return ConnectionGuard(*this);
  }

 private:
  WifiManager() = default;
  ~WifiManager();

  void deinitialize();
  static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

  std::atomic<bool> m_initialized{false};
  std::atomic<bool> m_connected{false};
  std::atomic<bool> m_connecting{false};

  WifiConfig m_config{};
  ConnectionInfo m_connection_info{};

  esp_netif_t* m_sta_netif{nullptr};
  esp_event_handler_instance_t m_wifi_event_handler{nullptr};
  esp_event_handler_instance_t m_ip_event_handler{nullptr};

  ConnectedCallback m_connected_callback;
  DisconnectedCallback m_disconnected_callback;
  ConnectionFailedCallback m_connection_failed_callback;
};

}  // namespace esp::wifi
