#pragma once

#include "app_config.h"
#include "esp_err.h"
#include "nas_status.h"

esp_err_t api_client_fetch_status(const app_config_t *config, nas_status_t *status);

