#pragma once

#include "nas_status.h"
#include "power_monitor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*ui_endpoint_save_cb_t)(const char *host, int port, void *user_data);

void ui_init(void);
void ui_set_endpoint_config(const char *host, int port);
void ui_set_endpoint_save_callback(ui_endpoint_save_cb_t callback, void *user_data);
void ui_set_web_url(const char *url);
void ui_set_power_status(const power_status_t *status);
void ui_set_message(const char *message);
void ui_update_status(const nas_status_t *status, bool online);
