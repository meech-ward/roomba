#pragma once

#include "new_socket_server.hpp"
#include "roomba.hpp"
auto camera_stream_task(void* arg) -> void;
auto handle_binary_message(httpd_handle_t& server, httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void;
auto handle_text_message(httpd_handle_t& server, httpd_ws_frame_t& ws_pkt, uint8_t* buf, int fd) -> void;
