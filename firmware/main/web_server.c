#include "web_server.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "web";

static httpd_handle_t s_server;
static app_config_t *s_config;
static web_config_update_cb_t s_update_cb;
static void *s_update_user_data;

static bool valid_host(const char *host)
{
    if (host == NULL || host[0] == '\0' || strlen(host) >= sizeof(s_config->api_host)) {
        return false;
    }
    for (const char *p = host; *p != '\0'; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (isspace(ch) || ch == '/' || ch == ':' || ch == '?' || ch == '#' ||
            ch == '"' || ch == '\'' || ch == '<' || ch == '>' || ch == '\\') {
            return false;
        }
    }
    return true;
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static void url_decode(char *text)
{
    char *read = text;
    char *write = text;
    while (*read != '\0') {
        if (*read == '+') {
            *write++ = ' ';
            read++;
        } else if (*read == '%' && isxdigit((unsigned char)read[1]) && isxdigit((unsigned char)read[2])) {
            int high = hex_value(read[1]);
            int low = hex_value(read[2]);
            *write++ = (char)((high << 4) | low);
            read += 3;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

static bool form_get(const char *body, const char *key, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return false;
    }
    out[0] = '\0';

    size_t key_len = strlen(key);
    const char *p = body;
    while (p != NULL && *p != '\0') {
        const char *next = strchr(p, '&');
        size_t pair_len = next ? (size_t)(next - p) : strlen(p);
        const char *eq = memchr(p, '=', pair_len);
        if (eq != NULL && (size_t)(eq - p) == key_len && strncmp(p, key, key_len) == 0) {
            size_t value_len = pair_len - key_len - 1;
            if (value_len >= out_size) {
                value_len = out_size - 1;
            }
            memcpy(out, eq + 1, value_len);
            out[value_len] = '\0';
            url_decode(out);
            return true;
        }
        p = next ? next + 1 : NULL;
    }
    return false;
}

static void html_escape(const char *input, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (input == NULL) {
        return;
    }

    size_t used = 0;
    for (const char *p = input; *p != '\0' && used + 1 < out_size; ++p) {
        const char *rep = NULL;
        switch (*p) {
        case '&':
            rep = "&amp;";
            break;
        case '<':
            rep = "&lt;";
            break;
        case '>':
            rep = "&gt;";
            break;
        case '"':
            rep = "&quot;";
            break;
        default:
            break;
        }

        if (rep != NULL) {
            size_t len = strlen(rep);
            if (used + len >= out_size) {
                break;
            }
            memcpy(out + used, rep, len);
            used += len;
        } else {
            out[used++] = *p;
        }
    }
    out[used] = '\0';
}

static esp_err_t redirect_home(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    char host[96];
    html_escape(s_config->api_host, host, sizeof(host));
    const bool has_token = s_config->api_token[0] != '\0';

    char *html = calloc(1, 20000);
    if (html == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
    }

    snprintf(html, 20000,
             "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
             "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
             "<title>NASflow 后台</title>"
             "<style>"
             ":root{font-family:-apple-system,BlinkMacSystemFont,'Noto Sans SC','Microsoft YaHei',sans-serif;color:#1f2b2a;background:#f4ecdf}"
             "*{box-sizing:border-box}body{margin:0;background:#f4ecdf;min-height:100vh}"
             "body:before{content:'';position:fixed;inset:0;background:linear-gradient(135deg,#fff7e9,#eaf7ff 52%%,#f6efff);z-index:-2}"
             "body:after{content:'';position:fixed;left:6vw;right:6vw;top:18px;height:8px;background:#ef6b5f;opacity:.18;border-radius:4px;z-index:-1}"
             ".wrap{max-width:1060px;margin:0 auto;padding:28px 18px 52px}.hero,.card{border:2px solid #22302e;border-radius:8px;background:#fffdf7;box-shadow:6px 7px 0 #d9caba}"
             ".hero{padding:26px;margin-bottom:18px;position:relative;overflow:hidden}.hero:before{content:'';position:absolute;left:24px;top:-4px;width:84px;height:10px;background:#4c96d7;border-radius:4px}"
             "h1{margin:0;font-size:30px;line-height:1.2;letter-spacing:0}h2{margin:0 0 14px;font-size:20px}.sub{color:#586966;margin-top:10px;line-height:1.7;max-width:760px}"
             ".grid{display:grid;grid-template-columns:1.08fr .92fr;gap:18px}.stack{display:grid;gap:18px;align-content:start}.card{padding:20px}.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:18px}"
             ".stat{border:2px solid #22302e;border-radius:8px;background:#f2fbff;padding:12px}.stat b{display:block;font-size:19px}.stat span{display:block;color:#66736f;font-size:13px;margin-top:4px}"
             "label{display:block;font-weight:800;margin:14px 0 7px}.hint{font-size:13px;color:#6b7774;margin-top:8px;line-height:1.6}.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}"
             "input,select{width:100%%;border:2px solid #1f2b2a;border-radius:8px;padding:13px 14px;font-size:16px;background:#fff;color:#1f2b2a}"
             "select{appearance:none;background:linear-gradient(90deg,#fff 0,#fff 70%%,#fff8dd 70%%);font-weight:800}"
             ".row{display:grid;grid-template-columns:2fr 1fr;gap:12px}.btn{border:2px solid #1f2b2a;border-radius:8px;background:#4c96d7;color:white;font-weight:800;font-size:15px;padding:13px 16px;box-shadow:4px 5px 0 #b9c9d9;cursor:pointer}"
             ".btn.secondary{background:#36aa69;box-shadow:4px 5px 0 #b8d4bf}.btn.ghost{background:#fff;color:#1f2b2a;box-shadow:none}.btn.danger{background:#e85d62;box-shadow:4px 5px 0 #dfb7b8}"
             ".pill{display:inline-block;border:2px solid #1f2b2a;border-radius:8px;background:#e9f7ee;padding:8px 12px;margin:8px 8px 0 0;font-weight:800}.pill.warn{background:#fff1cf}.pill.info{background:#eef4ff}"
             ".actions{display:flex;gap:12px;flex-wrap:wrap;margin-top:18px}.quick{display:flex;gap:8px;flex-wrap:wrap}.quick button{border:2px solid #22302e;border-radius:8px;background:#fff8dd;padding:8px 10px;font-weight:800;cursor:pointer}"
             ".result{border:2px dashed #22302e;border-radius:8px;background:#f6fbff;padding:12px;margin-top:14px;min-height:46px;color:#43514e}.api{display:grid;gap:8px}.api a{color:#1b6aa6;font-weight:800;text-decoration:none}"
             "@media(max-width:780px){.grid,.row,.stats{grid-template-columns:1fr}.hero{padding:22px}h1{font-size:26px}}"
             "</style></head><body><main class=\"wrap\">"
             "<section class=\"hero\"><h1>NASflow 控制台</h1><div class=\"sub\">屏幕负责漂亮展示，复杂输入和运维动作放在这里。保存后 ESP 会立即切换到新的 NAS Agent 地址。</div>"
             "<div class=\"stats\"><div class=\"stat\"><b class=\"mono\">%s:%d</b><span>当前目标</span></div><div class=\"stat\"><b>%lu ms</b><span>轮询间隔</span></div><div class=\"stat\"><b>%s</b><span>Token</span></div><div class=\"stat\"><b id=\"selfHost\">--</b><span>ESP 后台</span></div></div></section>"
             "<section class=\"grid\"><div class=\"stack\"><form class=\"card\" method=\"post\" action=\"/config\"><h2>连接设置</h2>"
             "<label>NAS Agent 主机地址</label><input name=\"host\" value=\"%s\" maxlength=\"63\" placeholder=\"192.168.101.12 或 nas.local\" required>"
             "<div class=\"row\"><div><label>端口</label><input name=\"port\" type=\"number\" min=\"1\" max=\"65535\" value=\"%d\" required></div>"
             "<div><label>刷新间隔 ms</label><input id=\"poll\" name=\"poll\" type=\"number\" min=\"1000\" max=\"60000\" step=\"500\" value=\"%lu\" required></div></div>"
             "<div class=\"quick\"><button type=\"button\" onclick=\"setPoll(1000)\">1 秒</button><button type=\"button\" onclick=\"setPoll(2000)\">2 秒</button><button type=\"button\" onclick=\"setPoll(5000)\">5 秒</button><button type=\"button\" onclick=\"setPoll(10000)\">10 秒</button></div>"
             "<label>API Token</label><input name=\"token\" type=\"password\" maxlength=\"95\" placeholder=\"%s\">"
             "<label><input name=\"clear_token\" type=\"checkbox\" value=\"1\" style=\"width:auto\"> 清空 Token</label>"
             "<div class=\"hint\">Token 留空表示保持不变；如需删除 Token，勾选清空。</div>"
             "<div class=\"actions\"><button class=\"btn\" type=\"submit\">保存设置</button></div></form></div>"
             "<div class=\"stack\"><section class=\"card\"><h2>连接诊断</h2><p class=\"hint\">由 ESP 直接请求 NAS Agent 的健康接口，最接近屏幕实际连通性。</p>"
             "<div class=\"actions\"><button class=\"btn secondary\" type=\"button\" onclick=\"testConn()\">测试连接</button><a class=\"btn ghost\" id=\"healthLink\" target=\"_blank\" href=\"#\">打开健康接口</a></div>"
             "<div id=\"testResult\" class=\"result\">尚未测试</div></section>"
             "<section class=\"card\"><h2>设备动作</h2><p class=\"hint\">配置会自动保存到 NVS。重启只在网络状态异常或需要重新初始化时使用。</p>"
             "<div class=\"actions\"><form method=\"post\" action=\"/restart\"><button class=\"btn danger\" type=\"submit\">重启设备</button></form><a class=\"btn ghost\" href=\"/api/config\" target=\"_blank\">查看配置 JSON</a></div></section>"
             "<section class=\"card\"><h2>后台 API</h2><div class=\"api\"><a href=\"/api/config\" target=\"_blank\">GET /api/config</a><a href=\"/api/test\" target=\"_blank\">GET /api/test</a><span class=\"hint\">接口不会回显 token 明文。</span></div></section></div></section>"
             "<script>"
             "const host='%s',port=%d;document.getElementById('selfHost').textContent=location.host;"
             "document.getElementById('healthLink').href='http://'+host+':'+port+'/api/v1/health';"
             "function setPoll(v){document.getElementById('poll').value=v}"
             "async function testConn(){const box=document.getElementById('testResult');box.textContent='正在测试...';try{const r=await fetch('/api/test',{cache:'no-store'});const j=await r.json();box.textContent=(j.ok?'连接正常 ':'连接失败 ')+j.status_code+' · '+j.message}catch(e){box.textContent='测试失败：'+e.message}}"
             "</script></main></body></html>",
             host, s_config->api_port, (unsigned long)s_config->poll_interval_ms, has_token ? "已设置" : "未设置",
             host, s_config->api_port, (unsigned long)s_config->poll_interval_ms, has_token ? "留空保持不变" : "可选",
             host, s_config->api_port);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    esp_err_t err = httpd_resp_sendstr(req, html);
    free(html);
    return err;
}

static esp_err_t api_config_get_handler(httpd_req_t *req)
{
    char json[256];
    snprintf(json, sizeof(json),
             "{\"api_host\":\"%s\",\"api_port\":%d,\"poll_interval_ms\":%lu,\"token_set\":%s}",
             s_config->api_host,
             s_config->api_port,
             (unsigned long)s_config->poll_interval_ms,
             s_config->api_token[0] ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t api_test_get_handler(httpd_req_t *req)
{
    char url[160];
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/health", s_config->api_host, s_config->api_port);

    esp_http_client_config_t client_config = {
        .url = url,
        .timeout_ms = 3500,
        .buffer_size = 256,
    };
    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    if (client == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "client init failed");
    }

    if (s_config->api_token[0] != '\0') {
        char auth[128];
        snprintf(auth, sizeof(auth), "Bearer %s", s_config->api_token);
        esp_http_client_set_header(client, "Authorization", auth);
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    bool ok = (err == ESP_OK && status >= 200 && status < 300);
    char json[256];
    snprintf(json, sizeof(json),
             "{\"ok\":%s,\"status_code\":%d,\"message\":\"%s\"}",
             ok ? "true" : "false",
             status,
             err == ESP_OK ? (ok ? "health ok" : "health returned non-2xx") : esp_err_to_name(err));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t config_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 1024) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
    }

    char body[1025];
    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, body + received, req->content_len - received);
        if (ret <= 0) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "receive failed");
        }
        received += ret;
    }
    body[received] = '\0';

    char host[64];
    char port_text[8];
    char poll_text[12];
    char token[96];
    char clear_token[4];
    if (!form_get(body, "host", host, sizeof(host)) ||
        !form_get(body, "port", port_text, sizeof(port_text)) ||
        !form_get(body, "poll", poll_text, sizeof(poll_text))) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing field");
    }
    form_get(body, "token", token, sizeof(token));
    bool clear = form_get(body, "clear_token", clear_token, sizeof(clear_token)) && strcmp(clear_token, "1") == 0;

    int port = atoi(port_text);
    uint32_t poll_ms = (uint32_t)strtoul(poll_text, NULL, 10);
    if (!valid_host(host) || port <= 0 || port > 65535 || poll_ms < 1000 || poll_ms > 60000) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid field");
    }

    app_config_t next = *s_config;
    strlcpy(next.api_host, host, sizeof(next.api_host));
    next.api_port = port;
    next.poll_interval_ms = poll_ms;
    if (clear) {
        next.api_token[0] = '\0';
    } else if (token[0] != '\0') {
        strlcpy(next.api_token, token, sizeof(next.api_token));
    }

    if (s_update_cb == NULL || !s_update_cb(&next, s_update_user_data)) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    }

    return redirect_home(req);
}

static esp_err_t restart_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_sendstr(req, "设备正在重启");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

esp_err_t web_server_start(app_config_t *config, web_config_update_cb_t callback, void *user_data)
{
    if (s_server != NULL) {
        return ESP_OK;
    }
    s_config = config;
    s_update_cb = callback;
    s_update_user_data = user_data;

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.server_port = 80;
    http_config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_server, &http_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to start server: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    const httpd_uri_t api_config = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = api_config_get_handler,
    };
    const httpd_uri_t api_test = {
        .uri = "/api/test",
        .method = HTTP_GET,
        .handler = api_test_get_handler,
    };
    const httpd_uri_t config_post = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = config_post_handler,
    };
    const httpd_uri_t restart_post = {
        .uri = "/restart",
        .method = HTTP_POST,
        .handler = restart_post_handler,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &api_config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &api_test));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &config_post));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &restart_post));
    ESP_LOGI(TAG, "web server started");
    return ESP_OK;
}
