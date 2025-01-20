idf_component_register(
    SRCS
        "new_socket_server.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_eth esp_http_server
)