idf_component_register(
    SRCS
        "main.cpp"
        "motor_command.cpp"
        "wifi_ap.cpp"
        "server_integration.cpp"
    INCLUDE_DIRS ""
    REQUIRES 
          gpio diagnostics camera server
          esp_wifi esp_timer openthread
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++23)
# target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_23)


