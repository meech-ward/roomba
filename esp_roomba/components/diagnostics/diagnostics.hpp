#pragma once
#include "esp_system.h"
#include "esp_wifi.h"

struct SystemStatus {
    float temperature;
    float voltage;
    uint32_t free_heap;
    int8_t wifi_tx_power;
    uint32_t cpu_frequency;
    wifi_bandwidth_t wifi_bandwidth;
    uint32_t total_internal_heap;
    uint32_t free_internal_heap;
    uint32_t total_psram;
    uint32_t free_psram;
    size_t free_stack;
};

using system_status_t = struct SystemStatus;


auto init_system_monitor() -> esp_err_t;
auto print_system_status(const system_status_t *status) -> void;
auto get_system_status(system_status_t *status) -> esp_err_t;
auto cleanup_system_monitor() -> void;
