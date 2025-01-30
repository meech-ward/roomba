idf_component_register(
    SRCS
       "roomba_core.cpp"
       "roomba_cleaning.cpp"
       "roomba_io.cpp"
       "roomba_movement.cpp"
       "roomba_sensors.cpp"
       "roomba_streaming.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_uart esp_timer
)