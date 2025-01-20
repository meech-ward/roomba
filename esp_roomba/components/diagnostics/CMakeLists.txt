idf_component_register(
    SRCS
        "diagnostics.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_adc esp_driver_gpio esp_driver_tsens esp_wifi
)