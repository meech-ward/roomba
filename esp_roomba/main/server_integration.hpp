#pragma once

#include "new_socket_server.hpp"

auto camera_stream_task(void* arg) -> void;
auto handle_binary_message(httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void;
auto handle_text_message(httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void;
