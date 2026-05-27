#include "app_config.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "app_config";
static const char *NVS_NAMESPACE = "nas_cfg";
static const char *KEY_API_HOST = "api_host";
static const char *KEY_API_PORT = "api_port";

void app_config_load(app_config_t *config)
{
    memset(config, 0, sizeof(*config));
    strlcpy(config->wifi_ssid, CONFIG_NAS_DISPLAY_WIFI_SSID, sizeof(config->wifi_ssid));
    strlcpy(config->wifi_password, CONFIG_NAS_DISPLAY_WIFI_PASSWORD, sizeof(config->wifi_password));
    strlcpy(config->api_host, CONFIG_NAS_DISPLAY_API_HOST, sizeof(config->api_host));
    config->api_port = CONFIG_NAS_DISPLAY_API_PORT;
    strlcpy(config->api_token, CONFIG_NAS_DISPLAY_API_TOKEN, sizeof(config->api_token));
    config->poll_interval_ms = CONFIG_NAS_DISPLAY_POLL_INTERVAL_MS;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return;
    }

    size_t host_len = sizeof(config->api_host);
    err = nvs_get_str(nvs, KEY_API_HOST, config->api_host, &host_len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "failed to read api host: %s", esp_err_to_name(err));
    }

    int32_t port = 0;
    err = nvs_get_i32(nvs, KEY_API_PORT, &port);
    if (err == ESP_OK && port > 0 && port <= 65535) {
        config->api_port = (int)port;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "failed to read api port: %s", esp_err_to_name(err));
    }

    nvs_close(nvs);
}

esp_err_t app_config_save_endpoint(const char *host, int port)
{
    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(nvs, KEY_API_HOST, host);
    if (err == ESP_OK) {
        err = nvs_set_i32(nvs, KEY_API_PORT, port);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
