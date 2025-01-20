idf_component_register(
    SRCS
        "motor.cpp"
        "pwm_controller.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_ledc nvs_flash
)