#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    char api_host[64];
    int api_port;
    char api_token[96];
    uint32_t poll_interval_ms;
} app_config_t;

void app_config_load(app_config_t *config);
esp_err_t app_config_save_endpoint(const char *host, int port);
esp_err_t app_config_save_api(const char *host, int port, const char *token, uint32_t poll_interval_ms);
esp_err_t app_config_save_all(const app_config_t *config);
