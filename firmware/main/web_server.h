#pragma once

#include "app_config.h"
#include "esp_err.h"

typedef bool (*web_config_update_cb_t)(const app_config_t *config, void *user_data);

esp_err_t web_server_start(app_config_t *config, web_config_update_cb_t callback, void *user_data);
