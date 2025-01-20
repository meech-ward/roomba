idf_component_register(
    SRCS
        "camera.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash esp32-camera esp_timer
)