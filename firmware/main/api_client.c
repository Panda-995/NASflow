#include "api_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "api";
#define API_BUFFER_SIZE (48 * 1024)

typedef struct {
    char *buffer;
    int length;
    int capacity;
} http_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_buffer_t *output = (http_buffer_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && output && evt->data && evt->data_len > 0) {
        int remaining = output->capacity - output->length - 1;
        int copy_len = evt->data_len < remaining ? evt->data_len : remaining;
        if (copy_len > 0) {
            memcpy(output->buffer + output->length, evt->data, copy_len);
            output->length += copy_len;
            output->buffer[output->length] = '\0';
        }
    }
    return ESP_OK;
}

esp_err_t api_client_fetch_status(const app_config_t *config, nas_status_t *status)
{
    char url[160];
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/status", config->api_host, config->api_port);

    char *buffer = calloc(1, API_BUFFER_SIZE);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    http_buffer_t output = {
        .buffer = buffer,
        .length = 0,
        .capacity = API_BUFFER_SIZE,
    };

    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .user_data = &output,
        .timeout_ms = 2500,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        free(buffer);
        return ESP_FAIL;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    if (config->api_token[0] != '\0') {
        char auth[128];
        snprintf(auth, sizeof(auth), "Bearer %s", config->api_token);
        esp_http_client_set_header(client, "Authorization", auth);
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || status_code != 200) {
        ESP_LOGW(TAG, "request failed err=%s status=%d", esp_err_to_name(err), status_code);
        free(buffer);
        return err == ESP_OK ? ESP_FAIL : err;
    }
    bool parsed = nas_status_parse_json(buffer, status);
    free(buffer);
    if (!parsed) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

