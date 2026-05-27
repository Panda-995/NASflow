#include "app_config.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "app_config";
static const char *NVS_NAMESPACE = "nas_cfg";
static const char *KEY_API_HOST = "api_host";
static const char *KEY_API_PORT = "api_port";
static const char *KEY_API_TOKEN = "api_token";
static const char *KEY_POLL_MS = "poll_ms";

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

    size_t token_len = sizeof(config->api_token);
    err = nvs_get_str(nvs, KEY_API_TOKEN, config->api_token, &token_len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "failed to read api token: %s", esp_err_to_name(err));
    }

    uint32_t poll_ms = 0;
    err = nvs_get_u32(nvs, KEY_POLL_MS, &poll_ms);
    if (err == ESP_OK && poll_ms >= 1000 && poll_ms <= 60000) {
        config->poll_interval_ms = poll_ms;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "failed to read poll interval: %s", esp_err_to_name(err));
    }

    nvs_close(nvs);
}

esp_err_t app_config_save_endpoint(const char *host, int port)
{
    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535) {
        return ESP_ERR_INVALID_ARG;
    }

    return app_config_save_api(host, port, NULL, 0);
}

esp_err_t app_config_save_api(const char *host, int port, const char *token, uint32_t poll_interval_ms)
{
    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535) {
        return ESP_ERR_INVALID_ARG;
    }
    if (poll_interval_ms != 0 && (poll_interval_ms < 1000 || poll_interval_ms > 60000)) {
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
    if (err == ESP_OK && token != NULL) {
        err = nvs_set_str(nvs, KEY_API_TOKEN, token);
    }
    if (err == ESP_OK && poll_interval_ms != 0) {
        err = nvs_set_u32(nvs, KEY_POLL_MS, poll_interval_ms);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
