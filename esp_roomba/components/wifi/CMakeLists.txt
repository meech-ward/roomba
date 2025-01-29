idf_component_register(
    SRCS
       "wifi_manager.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash
)