#include "wifi_ap.hpp"

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>

#include <cstring>

#define WIFI_AP_SSID "ESP-AP"
#define WIFI_AP_PASS "myapp1234"
#define MAX_CONNECTIONS 1
constexpr uint16_t beacon_interval = 100;

// hotspot
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
auto setup_wifi() -> void {
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));

  wifi_config_t wifi_config = {
    .ap =
      {
        .ssid = WIFI_AP_SSID,
        .password = WIFI_AP_PASS,
        .ssid_len = static_cast<uint8_t>(strlen(WIFI_AP_SSID)),
        .channel = 1,
        .authmode = WIFI_AUTH_WPA2_PSK,
        .ssid_hidden = 0,
        .max_connection = MAX_CONNECTIONS,
        .beacon_interval = beacon_interval,
        .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP,
        .ftm_responder = false,
        .pmf_cfg =
          {
            .capable = true,
            .required = true,
          },
        .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
      },
  };

  // Set WiFi country to match iOS device (example for US)
  esp_wifi_set_country_code("CA", true);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
  ESP_LOGI("WIFI", "AP Started with IP: " IPSTR, IP2STR(&ip_info.ip));

  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_AP_STACONNECTED: {
        auto* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI("WIFI", "Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);

        // Log current connection count
        wifi_sta_list_t sta_list;
        esp_wifi_ap_get_sta_list(&sta_list);
        ESP_LOGI("WIFI", "Current number of connected stations: %u", sta_list.num);
        break;
      }

      case WIFI_EVENT_AP_STADISCONNECTED: {
        auto* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI("WIFI", "Station " MACSTR " left, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);

        // Log current connection count
        wifi_sta_list_t sta_list;
        esp_wifi_ap_get_sta_list(&sta_list);
        ESP_LOGI("WIFI", "Current number of connected stations: %u", sta_list.num);
        break;
      }

      case WIFI_EVENT_AP_STOP: {
        ESP_LOGI("WIFI", "AP Stop");
        break;
      }

      case WIFI_EVENT_AP_PROBEREQRECVED: {
        auto* event = (wifi_event_ap_probe_req_rx_t*)event_data;
        ESP_LOGD("WIFI", "Probe request from: " MACSTR ", RSSI=%d", MAC2STR(event->mac), event->rssi);
        break;
      }

      default:
        ESP_LOGI("WIFI", "Unhandled WiFi event: %" PRId32, event_id);
        break;
    }
  }
}