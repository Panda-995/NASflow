#include "app_config.h"
#include "api_client.h"
#include "board_5b.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "ui.h"
#include "wifi_manager.h"

#include <string.h>

static const char *TAG = "app";
static app_config_t s_config;
static nas_status_t s_status;

static bool endpoint_save_cb(const char *host, int port, void *user_data)
{
    (void)user_data;
    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535) {
        return false;
    }

    esp_err_t err = app_config_save_endpoint(host, port);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to save endpoint: %s", esp_err_to_name(err));
        return false;
    }

    strlcpy(s_config.api_host, host, sizeof(s_config.api_host));
    s_config.api_port = port;
    ESP_LOGI(TAG, "api endpoint updated");
    return true;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    app_config_load(&s_config);

    ESP_ERROR_CHECK(board_5b_init());
    ui_set_endpoint_config(s_config.api_host, s_config.api_port);
    ui_set_endpoint_save_callback(endpoint_save_cb, NULL);
    ui_init();

    if (wifi_manager_start(s_config.wifi_ssid, s_config.wifi_password) == ESP_OK) {
        ui_set_message("正在连接 Wi-Fi");
        if (wifi_manager_wait_connected(15000)) {
            ui_set_message("Wi-Fi 已连接");
        } else {
            ui_set_message("Wi-Fi 连接超时");
        }
    } else {
        ui_set_message("请在 menuconfig 配置 Wi-Fi");
    }

    nas_status_init(&s_status);
    while (true) {
        if (!wifi_manager_is_connected()) {
            ui_update_status(&s_status, false);
            vTaskDelay(pdMS_TO_TICKS(s_config.poll_interval_ms));
            continue;
        }

        err = api_client_fetch_status(&s_config, &s_status);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "status updated");
            ui_update_status(&s_status, true);
        } else {
            ESP_LOGW(TAG, "status fetch failed: %s", esp_err_to_name(err));
            ui_update_status(&s_status, false);
        }
        vTaskDelay(pdMS_TO_TICKS(s_config.poll_interval_ms));
    }
}
