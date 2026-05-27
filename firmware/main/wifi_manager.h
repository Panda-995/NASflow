#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

esp_err_t wifi_manager_start(const char *ssid, const char *password);
bool wifi_manager_wait_connected(int timeout_ms);
bool wifi_manager_is_connected(void);
void wifi_manager_get_ip(char *out, size_t out_size);
