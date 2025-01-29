// wifi_manager.cpp
#include "wifi_manager.hpp"

#include <cstdio>

#include "esp_log.h"
#include "nvs_flash.h"

namespace wifi {

namespace {
constexpr const char* TAG = "WifiManager";

auto get_disconnect_reason_str(uint8_t reason) -> const char* {
  switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
      return "Unspecified";
    case WIFI_REASON_AUTH_EXPIRE:
      return "Auth Expired";
    case WIFI_REASON_AUTH_LEAVE:
      return "Auth Leave";
    case WIFI_REASON_ASSOC_EXPIRE:
      return "Association Expired";
    case WIFI_REASON_ASSOC_TOOMANY:
      return "Too Many Associations";
    case WIFI_REASON_NOT_AUTHED:
      return "Not Authenticated";
    case WIFI_REASON_NOT_ASSOCED:
      return "Not Associated";
    case WIFI_REASON_ASSOC_LEAVE:
      return "Association Leave";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
      return "Association Not Authenticated";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
      return "Bad Power Capability";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
      return "Bad Supported Channels";
    case WIFI_REASON_IE_INVALID:
      return "Invalid IE";
    case WIFI_REASON_MIC_FAILURE:
      return "MIC Failure";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      return "4-Way Handshake Timeout";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
      return "Group Key Update Timeout";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
      return "IE In 4-Way Handshake Differs";
    case WIFI_REASON_GROUP_CIPHER_INVALID:
      return "Invalid Group Cipher";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
      return "Invalid Pairwise Cipher";
    case WIFI_REASON_AKMP_INVALID:
      return "Invalid AKMP";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
      return "Unsupported RSN IE Version";
    case WIFI_REASON_INVALID_RSN_IE_CAP:
      return "Invalid RSN IE Capability";
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return "802.1X Authentication Failed";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
      return "Cipher Suite Rejected";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "Beacon Timeout";
    case WIFI_REASON_NO_AP_FOUND:
      return "No AP Found";
    case WIFI_REASON_AUTH_FAIL:
      return "Authentication Failed";
    case WIFI_REASON_ASSOC_FAIL:
      return "Association Failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "Handshake Timeout";
    case WIFI_REASON_CONNECTION_FAIL:
      return "Connection Failed";
    default:
      return "Unknown";
  }
}

[[nodiscard]] auto initialize_nvs() -> std::expected<void, WifiError> {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    return std::unexpected(WifiError::SystemError);
  }
  return {};
}
}  // namespace

auto WifiManager::instance() -> WifiManager& {
  static WifiManager instance;
  return instance;
}

WifiManager::ConnectionGuard::ConnectionGuard(WifiManager& manager) : m_manager(manager) {
  m_result = m_manager.connect();
}
WifiManager::ConnectionGuard::~ConnectionGuard() {
  [[maybe_unused]] auto result = m_manager.disconnect();
}

WifiManager::~WifiManager() {
  deinitialize();
}

void WifiManager::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  auto* manager = static_cast<WifiManager*>(arg);
  if (manager == nullptr) {
    return;
  }

  if (event_base == WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "Attempting to connect to SSID: %s", manager->m_config.ssid.c_str());
        esp_wifi_connect();
        break;

      case WIFI_EVENT_STA_CONNECTED: {
        auto* event = static_cast<wifi_event_sta_connected_t*>(event_data);
        manager->m_connected = true;
        ESP_LOGI(TAG, "Successfully connected to SSID: %s", manager->m_config.ssid.c_str());
        ESP_LOGI(TAG, "Channel: %d, Auth Mode: %d", event->channel, event->authmode);

        if (manager->m_connected_callback) {
          wifi_ap_record_t ap_info;
          if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            manager->m_connection_info.rssi = ap_info.rssi;
            manager->m_connection_info.auth_mode = ap_info.authmode;
            manager->m_connection_info.ssid = reinterpret_cast<char*>(ap_info.ssid);
            ESP_LOGI(TAG, "Signal strength (RSSI): %d dBm", ap_info.rssi);
            manager->m_connected_callback(manager->m_connection_info);

            if (manager->m_connection_info.ip_info.ip.addr != 0) {
              manager->wifi_ready = true;
            }
          }
        }
        break;
      }

      case WIFI_EVENT_STA_DISCONNECTED: {
        auto* event = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        manager->m_connected = false;

        const char* reason_str = get_disconnect_reason_str(event->reason);
        ESP_LOGW(TAG, "Disconnected from SSID: %s", manager->m_config.ssid.c_str());
        ESP_LOGW(TAG, "Reason: %s (Code: %d)", reason_str, event->reason);

        WifiError error;
        switch (event->reason) {
          case WIFI_REASON_AUTH_FAIL:
          case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            ESP_LOGW(TAG, "Authentication failed. Please check your password");
            error = WifiError::InvalidCredentials;
            break;
          case WIFI_REASON_NO_AP_FOUND:
            ESP_LOGW(TAG, "Network not found. Please check SSID: %s", manager->m_config.ssid.c_str());
            error = WifiError::NetworkNotFound;
            break;
          case WIFI_REASON_BEACON_TIMEOUT:
          case WIFI_REASON_HANDSHAKE_TIMEOUT:
            ESP_LOGW(TAG, "Weak signal or network issues detected");
            error = WifiError::WeakSignal;
            break;
          default:
            error = WifiError::ConnectionFailed;
        }

        if (manager->m_connecting) {
          if (manager->m_connection_failed_callback) {
            manager->m_connection_failed_callback(error);
          }
          ESP_LOGI(TAG, "Retrying connection...");
          esp_wifi_connect();
        } else if (manager->m_disconnected_callback) {
          manager->m_disconnected_callback(error);
        }
        break;
      }
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    auto* event = static_cast<ip_event_got_ip_t*>(event_data);
    manager->m_connection_info.ip_info = event->ip_info;
    manager->m_connection_info.connected = true;

    ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
    ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));

    if (manager->m_connected_callback) {
      manager->m_connected_callback(manager->m_connection_info);
      if (manager->m_connection_info.ip_info.ip.addr != 0) {
        manager->wifi_ready = true;
      }
    }
  }
}

auto WifiManager::initialize_for_scan() -> std::expected<void, WifiError> {
  if (m_initialized) {
    ESP_LOGW(TAG, "WiFi manager already initialized");
    return std::unexpected(WifiError::AlreadyInitialized);
  }

  ESP_LOGI(TAG, "Initializing WiFi manager for scanning");

  // Initialize NVS
  if (auto result = initialize_nvs(); !result) {
    return result;
  }

  // Create default station interface
  m_sta_netif = esp_netif_create_default_wifi_sta();
  if (m_sta_netif == nullptr) {
    ESP_LOGE(TAG, "Failed to create station interface");
    return std::unexpected(WifiError::SystemError);
  }

  // Initialize WiFi with default config
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&cfg) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize WiFi");
    return std::unexpected(WifiError::SystemError);
  }

  // Set mode to station
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  // Start WiFi
  ESP_ERROR_CHECK(esp_wifi_start());

  m_initialized = true;
  return {};
}

auto WifiManager::scan_networks() -> std::expected<void, WifiError> {
  if (!m_initialized) {
    return std::unexpected(WifiError::NotInitialized);
  }

  ESP_LOGI(TAG, "Starting network scan...");

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  vTaskDelay(pdMS_TO_TICKS(100));

  wifi_scan_config_t scan_config = {.ssid = nullptr, .bssid = nullptr, .channel = 0, .show_hidden = true};

  if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start scan");
    return std::unexpected(WifiError::SystemError);
  }

  uint16_t ap_count = 0;
  esp_wifi_scan_get_ap_num(&ap_count);
  ESP_LOGI(TAG, "Found %d networks", ap_count);

  if (ap_count > 0) {
    std::vector<wifi_ap_record_t> ap_records(ap_count);
    if (esp_wifi_scan_get_ap_records(&ap_count, ap_records.data()) == ESP_OK) {
      for (int i = 0; i < ap_count; i++) {
        ESP_LOGI(TAG, "Network %d:", i);
        ESP_LOGI(TAG, "    SSID: %s", (char*)ap_records[i].ssid);
        ESP_LOGI(TAG, "    Channel: %d", ap_records[i].primary);
        ESP_LOGI(TAG, "    RSSI: %d", ap_records[i].rssi);
        ESP_LOGI(TAG, "    Auth mode: %d", ap_records[i].authmode);
      }
    }
  }

  return {};
}

auto WifiManager::initialize(const WifiConfig& config) -> std::expected<void, WifiError> {
  if (m_initialized) {
    ESP_LOGW(TAG, "WiFi manager already initialized");
    return std::unexpected(WifiError::AlreadyInitialized);
  }

  ESP_LOGI(TAG, "Initializing WiFi manager");

  if (config.ssid.empty()) {
    ESP_LOGE(TAG, "SSID cannot be empty");
    return std::unexpected(WifiError::InvalidConfiguration);
  }

  if (auto result = initialize_nvs(); !result) {
    return result;
  }

  m_config = config;

  m_sta_netif = esp_netif_create_default_wifi_sta();
  if (m_sta_netif == nullptr) {
    ESP_LOGE(TAG, "Failed to create station interface");
    return std::unexpected(WifiError::SystemError);
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&cfg) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize WiFi");
    return std::unexpected(WifiError::SystemError);
  }

  esp_event_handler_instance_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::event_handler, this, &m_wifi_event_handler);
  esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiManager::event_handler, this, &m_ip_event_handler);

  ESP_LOGI(TAG, "WiFi manager initialized successfully");
  m_initialized = true;
  return {};
}

auto WifiManager::connect() -> std::expected<void, WifiError> {
  if (!m_initialized) {
    return std::unexpected(WifiError::NotInitialized);
  }

  if (m_connected) {
    return {};
  }

  m_connecting = true;

  wifi_config_t wifi_config = {};
  std::copy_n(
    m_config.ssid.data(), std::min(m_config.ssid.size(), size_t{32}), reinterpret_cast<char*>(wifi_config.sta.ssid));
  std::copy_n(
    m_config.password.data(),
    std::min(m_config.password.size(), size_t{64}),
    reinterpret_cast<char*>(wifi_config.sta.password));

  wifi_config.sta.scan_method = m_config.scan_method;
  wifi_config.sta.threshold.rssi = m_config.min_rssi;
  wifi_config.sta.threshold.authmode = m_config.min_authmode;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_set_ps(m_config.power_save));
  ESP_ERROR_CHECK(esp_wifi_start());

  return {};
}

auto WifiManager::disconnect() -> std::expected<void, WifiError> {
  if (!m_initialized) {
    return std::unexpected(WifiError::NotInitialized);
  }

  m_connecting = false;
  ESP_ERROR_CHECK(esp_wifi_disconnect());
  ESP_ERROR_CHECK(esp_wifi_stop());

  return {};
}

auto WifiManager::get_connection_info() const -> std::expected<ConnectionInfo, WifiError> {
  if (!m_initialized) {
    return std::unexpected(WifiError::NotInitialized);
  }

  if (!m_connected) {
    return std::unexpected(WifiError::ConnectionFailed);
  }

  return m_connection_info;
}

void WifiManager::deinitialize() {
  if (!m_initialized) {
    return;
}

  if (auto result = disconnect(); !result) {
    // Ignore error since we're deinitializing anyway
  }

  if (m_wifi_event_handler != nullptr) {
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, m_wifi_event_handler);
  }
  if (m_ip_event_handler != nullptr) {
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, m_ip_event_handler);
  }

  esp_wifi_deinit();

  if (m_sta_netif != nullptr) {
    esp_netif_destroy(m_sta_netif);
  }

  esp_netif_deinit();

  m_initialized = false;
}

}  // namespace esp::wifi