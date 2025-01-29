
#include "new_socket_server.hpp"

#include <esp_http_server.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#include <cstdio>
#include <cstring>

#include "hal/ledc_types.h"
#include "soc/gpio_num.h"

namespace server {

static const char* TAG = "ws_server";

static httpd_handle_t s_server = nullptr;

// ----------------------- WebSocket Handler ----------------------

static WsMessageHandler g_ws_binary_handler = nullptr;
static WsMessageHandler g_ws_text_handler = nullptr;

auto set_ws_binary_handler(WsMessageHandler handler) -> void {
  g_ws_binary_handler = handler;
}
auto set_ws_text_handler(WsMessageHandler handler) -> void {
  g_ws_text_handler = handler;
}

static auto ws_handler(httpd_req_t* req) -> esp_err_t {
  // HTTP GET means handshake
  if (req->method == HTTP_GET) {
    // Disable Nagle to reduce latency on image stream
    int fd = httpd_req_to_sockfd(req);
    int yes = 1;
    lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

    ESP_LOGI(TAG, "WS handshake done, new connection (fd=%d)", httpd_req_to_sockfd(req));
    return ESP_OK;
  }
  // For actual data frames from the client
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get ws frame len: %d", ret);
    return ret;
  }

  if (ws_pkt.len == 0) {
    return ESP_OK;  // No payload
  }

  auto* buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
  if (buf == nullptr) {
    ESP_LOGE(TAG, "No mem for WS buffer");
    return ESP_ERR_NO_MEM;
  }
  ws_pkt.payload = buf;

  ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to recv WS frame payload: %d", ret);
    free(buf);
    return ret;
  }

  int fd = httpd_req_to_sockfd(req);
  if (ws_pkt.type == HTTPD_WS_TYPE_BINARY) {
    // Handle binary motor commands
    g_ws_binary_handler(ws_pkt, buf, fd);
  } else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
    g_ws_text_handler(ws_pkt, buf, fd);
  }

  free(buf);
  return ret;
}

// ----------------------- Web Server Setup -----------------------
auto start_webserver() -> httpd_handle_t {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.recv_wait_timeout = 4;    // Reduce from default
  config.send_wait_timeout = 4;    // Reduce from default
  config.max_uri_handlers = 1;     // We only need one
  config.max_open_sockets = 1;     // For single client
  config.lru_purge_enable = true;  // Enable purging of old packets
  config.backlog_conn = 1;         // Minimum connection backlog
  // Optionally tweak for performance:
  // config.stack_size = 32768;
  // config.recv_wait_timeout = ...
  // config.send_wait_timeout = ...
  config.core_id = 0;

  ESP_LOGI(TAG, "Starting HTTP WS server on port %d", config.server_port);
  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t ws_uri = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = ws_handler,
      .user_ctx = nullptr,
      .is_websocket = true,
      .handle_ws_control_frames = false,
      .supported_subprotocol = nullptr};
    httpd_register_uri_handler(server, &ws_uri);
    ESP_LOGI(TAG, "WS /ws handler registered");
  } else {
    ESP_LOGE(TAG, "Error starting server! So Restarting");
    esp_restart();
  }
  s_server = server;

  return server;
}
}  // namespace server