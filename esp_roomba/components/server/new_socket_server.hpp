#pragma once

#include <esp_http_server.h>

namespace server {

auto start_webserver() -> httpd_handle_t;
auto broadcast_message(httpd_handle_t hd, const char* message) -> void;

// httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd
using WsMessageHandler = void (*)(httpd_ws_frame_t&, uint8_t*, int);
auto set_ws_binary_handler(WsMessageHandler handler) -> void;
auto set_ws_text_handler(WsMessageHandler handler) -> void;
}  // namespace server

