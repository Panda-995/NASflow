#include "ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "backgrounds/page_backgrounds.h"
#include "board_5b.h"
#include "lvgl.h"
#include "sdkconfig.h"

typedef enum {
    PAGE_HOME = 0,
    PAGE_PERF,
    PAGE_STORAGE,
    PAGE_DRIVES,
    PAGE_NETWORK,
    PAGE_SERVICES,
    PAGE_SETTINGS,
    PAGE_COUNT,
} ui_page_t;

typedef struct {
    lv_color_t accent;
    lv_color_t bg;
    const char *name;
} page_theme_t;

LV_FONT_DECLARE(lv_font_nas_cn_18);
#define FONT_CN (&lv_font_nas_cn_18)

#define SCREEN_W 1024
#define SCREEN_H 600
#define HEADER_H 66
#define CONTENT_H (SCREEN_H - HEADER_H)
#define UI_LOCK_TIMEOUT_MS 1000

static const page_theme_t PAGE_THEME[PAGE_COUNT] = {
    [PAGE_HOME] = {LV_COLOR_MAKE(0xEF, 0x6B, 0x5F), LV_COLOR_MAKE(0xFF, 0xF4, 0xE3), "总览"},
    [PAGE_PERF] = {LV_COLOR_MAKE(0x22, 0x9B, 0x97), LV_COLOR_MAKE(0xE9, 0xF8, 0xF0), "性能"},
    [PAGE_STORAGE] = {LV_COLOR_MAKE(0xF1, 0xB9, 0x3F), LV_COLOR_MAKE(0xFF, 0xF7, 0xD8), "存储"},
    [PAGE_DRIVES] = {LV_COLOR_MAKE(0x70, 0x8A, 0xE8), LV_COLOR_MAKE(0xEF, 0xF3, 0xFF), "硬盘"},
    [PAGE_NETWORK] = {LV_COLOR_MAKE(0x36, 0xAA, 0x69), LV_COLOR_MAKE(0xEA, 0xF8, 0xEA), "网络"},
    [PAGE_SERVICES] = {LV_COLOR_MAKE(0x89, 0x67, 0xC8), LV_COLOR_MAKE(0xF4, 0xEF, 0xFF), "服务"},
    [PAGE_SETTINGS] = {LV_COLOR_MAKE(0x4B, 0x95, 0xD9), LV_COLOR_MAKE(0xEA, 0xF7, 0xFF), "后台"},
};

static lv_obj_t *s_header;
static lv_obj_t *s_content;
static lv_obj_t *s_title_label;
static lv_obj_t *s_hint_label;
static lv_obj_t *s_online_badge;
static lv_obj_t *s_page_label;
static lv_obj_t *s_page_dots[PAGE_COUNT];

static ui_page_t s_page = PAGE_HOME;
static nas_status_t s_status;
static bool s_has_status;
static bool s_online;
static char s_message[96] = "正在启动";
static char s_endpoint_host[64] = CONFIG_NAS_DISPLAY_API_HOST;
static int s_endpoint_port = CONFIG_NAS_DISPLAY_API_PORT;
static char s_web_url[64] = "连接 Wi-Fi 后显示";
static ui_endpoint_save_cb_t s_save_cb;
static void *s_save_user_data;

static lv_style_t s_style_screen;
static lv_style_t s_style_header;
static lv_style_t s_style_card;
static lv_style_t s_style_chip;

typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    float used_pct;
    int count;
} storage_summary_t;

static const char *safe_text(const char *text, const char *fallback)
{
    return text != NULL && text[0] != '\0' ? text : fallback;
}

static bool text_equals(const char *text, const char *value)
{
    return text != NULL && strcasecmp(text, value) == 0;
}

static const char *health_text(const char *value)
{
    if (value == NULL || value[0] == '\0' || text_equals(value, "unknown")) {
        return "未知";
    }
    if (text_equals(value, "ok") || text_equals(value, "good") || text_equals(value, "healthy") ||
        text_equals(value, "passed") || text_equals(value, "clean") || text_equals(value, "online") ||
        text_equals(value, "running") || text_equals(value, "up") || text_equals(value, "active")) {
        return "正常";
    }
    if (text_equals(value, "warning") || text_equals(value, "warn") || text_equals(value, "degraded")) {
        return "注意";
    }
    if (text_equals(value, "stopped") || text_equals(value, "exited")) {
        return "停止";
    }
    if (text_equals(value, "critical") || text_equals(value, "failed") || text_equals(value, "unhealthy") ||
        text_equals(value, "error") || text_equals(value, "down")) {
        return "异常";
    }
    return "未知";
}

static const char *state_text(const char *value)
{
    if (text_equals(value, "running") || text_equals(value, "up") || text_equals(value, "online") ||
        text_equals(value, "active")) {
        return "运行";
    }
    if (text_equals(value, "stopped") || text_equals(value, "exited") || text_equals(value, "down") ||
        text_equals(value, "disconnected")) {
        return "停止";
    }
    if (value == NULL || value[0] == '\0' || text_equals(value, "unknown")) {
        return "未知";
    }
    return value;
}

static lv_color_t health_color(const char *health)
{
    if (text_equals(health, "ok") || text_equals(health, "good") || text_equals(health, "healthy") ||
        text_equals(health, "passed") || text_equals(health, "clean") || text_equals(health, "online") ||
        text_equals(health, "running") || text_equals(health, "up") || text_equals(health, "active")) {
        return lv_color_hex(0x35A86D);
    }
    if (text_equals(health, "warning") || text_equals(health, "warn") || text_equals(health, "degraded")) {
        return lv_color_hex(0xEBA83A);
    }
    if (text_equals(health, "critical") || text_equals(health, "failed") || text_equals(health, "unhealthy") ||
        text_equals(health, "error") || text_equals(health, "down")) {
        return lv_color_hex(0xE85D62);
    }
    return lv_color_hex(0x87928F);
}

static int pct_i(float value)
{
    if (value < 0.0f) {
        return 0;
    }
    if (value > 100.0f) {
        return 100;
    }
    return (int)(value + 0.5f);
}

static void format_pct(float value, char *out, size_t out_size)
{
    if (value < 0.0f) {
        snprintf(out, out_size, "--");
    } else {
        snprintf(out, out_size, "%.0f%%", value);
    }
}

static void format_temp(float value, char *out, size_t out_size)
{
    if (value <= 0.0f) {
        snprintf(out, out_size, "--");
    } else {
        snprintf(out, out_size, "%.0f℃", value);
    }
}

static void format_uptime(uint32_t seconds, char *out, size_t out_size)
{
    uint32_t days = seconds / 86400;
    uint32_t hours = (seconds % 86400) / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    if (days > 0) {
        snprintf(out, out_size, "%lu天 %lu小时", (unsigned long)days, (unsigned long)hours);
    } else if (hours > 0) {
        snprintf(out, out_size, "%lu小时 %lu分", (unsigned long)hours, (unsigned long)minutes);
    } else {
        snprintf(out, out_size, "%lu分钟", (unsigned long)minutes);
    }
}

static storage_summary_t storage_summary(void)
{
    storage_summary_t summary = {0};
    summary.count = s_status.storage.pool_count;

    if (s_status.storage.pool_count > 0) {
        for (int i = 0; i < s_status.storage.pool_count; ++i) {
            const nas_pool_t *pool = &s_status.storage.pools[i];
            summary.total_bytes += pool->total_bytes;
            summary.used_bytes += pool->used_bytes;
            summary.free_bytes += pool->free_bytes;
        }
    } else {
        summary.count = s_status.storage.volume_count;
        for (int i = 0; i < s_status.storage.volume_count; ++i) {
            const nas_volume_t *vol = &s_status.storage.volumes[i];
            summary.total_bytes += vol->total_bytes;
            summary.used_bytes += vol->used_bytes;
            summary.free_bytes += vol->free_bytes;
        }
    }

    if (summary.used_bytes == 0 && summary.total_bytes > summary.free_bytes) {
        summary.used_bytes = summary.total_bytes - summary.free_bytes;
    }
    if (summary.free_bytes == 0 && summary.total_bytes > summary.used_bytes) {
        summary.free_bytes = summary.total_bytes - summary.used_bytes;
    }
    summary.used_pct = summary.total_bytes > 0 ? (float)summary.used_bytes * 100.0f / (float)summary.total_bytes : -1.0f;
    return summary;
}

static int connected_interface_count(void)
{
    int count = 0;
    for (int i = 0; i < s_status.network.interface_count; ++i) {
        if (text_equals(s_status.network.interfaces[i].status, "up") ||
            text_equals(s_status.network.interfaces[i].status, "online")) {
            count++;
        }
    }
    return count;
}

static const char *interface_ip(const nas_interface_t *iface, int index)
{
    if (iface != NULL && iface->ip[0] != '\0') {
        return iface->ip;
    }
    if (iface != NULL && (text_equals(iface->status, "up") || text_equals(iface->status, "online")) &&
        s_status.nas.primary_ip[0] != '\0') {
        return s_status.nas.primary_ip;
    }
    return "未分配 IP";
}

static void format_link_speed(const nas_interface_t *iface, char *out, size_t out_size)
{
    if (iface == NULL || iface->link_speed_mbps <= 0 || text_equals(iface->status, "disconnected") ||
        text_equals(iface->status, "down")) {
        snprintf(out, out_size, "未接入");
        return;
    }
    if (iface->link_speed_mbps >= 1000) {
        int whole = iface->link_speed_mbps / 1000;
        int tenth = (iface->link_speed_mbps % 1000) / 100;
        if (tenth > 0) {
            snprintf(out, out_size, "%d.%dG", whole, tenth);
        } else {
            snprintf(out, out_size, "%dG", whole);
        }
    } else {
        snprintf(out, out_size, "%dM", iface->link_speed_mbps);
    }
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, FONT_CN, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_letter_space(obj, 0, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    if (w > 0) {
        lv_obj_set_width(obj, w);
    }
    return obj;
}

static lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t accent, lv_color_t bg)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_add_style(obj, &s_style_card, 0);
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_border_color(obj, accent, 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *rail = lv_obj_create(obj);
    lv_obj_remove_style_all(rail);
    lv_obj_set_style_bg_color(rail, accent, 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rail, 4, 0);
    lv_obj_set_pos(rail, 0, 0);
    lv_obj_set_size(rail, 8, h);

    lv_obj_t *tape = lv_obj_create(obj);
    lv_obj_remove_style_all(tape);
    lv_obj_set_style_bg_color(tape, accent, 0);
    lv_obj_set_style_bg_opa(tape, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tape, 4, 0);
    lv_obj_set_pos(tape, 22, -4);
    lv_obj_set_size(tape, 72, 10);
    return obj;
}

static void mark(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_radius(obj, 4, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

static void draw_background_marks(lv_color_t accent)
{
    mark(s_content, 24, 18, 118, 7, accent, LV_OPA_30);
    mark(s_content, 850, 28, 132, 7, lv_color_hex(0xEF6B5F), LV_OPA_30);
    mark(s_content, 930, 468, 64, 7, accent, LV_OPA_30);
    mark(s_content, 44, 494, 86, 7, lv_color_hex(0x36AA69), LV_OPA_30);
    mark(s_content, 982, 120, 9, 120, accent, LV_OPA_20);
}

static void draw_page_background(void)
{
    lv_obj_set_style_bg_color(s_content, PAGE_THEME[s_page].bg, 0);

    lv_obj_t *img = lv_img_create(s_content);
    lv_img_set_src(img, &bg_page_1);
    lv_obj_set_pos(img, 0, -1);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *wash = lv_obj_create(s_content);
    lv_obj_remove_style_all(wash);
    lv_obj_set_pos(wash, 0, 0);
    lv_obj_set_size(wash, SCREEN_W, CONTENT_H);
    lv_obj_set_style_bg_color(wash, PAGE_THEME[s_page].bg, 0);
    lv_obj_set_style_bg_opa(wash, LV_OPA_40, 0);
    lv_obj_clear_flag(wash, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wash, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

static void chip(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_add_style(obj, &s_style_chip, 0);
    lv_obj_set_style_bg_color(obj, lv_color_mix(color, lv_color_white(), 180), 0);
    lv_obj_set_style_border_color(obj, color, 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, 38);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *txt = label(obj, text, lv_color_hex(0x1E2C2A), 0, 8, w);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
}

static void metric(lv_obj_t *parent, const char *name, const char *value, int x, int y, int w, lv_color_t accent)
{
    label(parent, name, lv_color_hex(0x65726F), x, y, w);
    lv_obj_t *v = label(parent, value, lv_color_hex(0x1C2927), x, y + 32, w);
    lv_obj_set_style_text_color(v, accent, 0);
}

static void bar(lv_obj_t *parent, int x, int y, int w, int value, lv_color_t accent)
{
    lv_obj_t *obj = lv_bar_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, 18);
    lv_bar_set_range(obj, 0, 100);
    lv_bar_set_value(obj, value, LV_ANIM_OFF);
    lv_obj_set_style_radius(obj, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xF2E3D0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
}

static void draw_empty(const char *text)
{
    const page_theme_t *theme = &PAGE_THEME[s_page];
    lv_obj_t *obj = card(s_content, 252, 148, 520, 210, theme->accent, lv_color_hex(0xFFFDF8));
    label(obj, "等待 NAS 数据", lv_color_hex(0x1C2927), 32, 44, 420);
    label(obj, text, lv_color_hex(0x61706C), 32, 94, 430);
    chip(obj, "请先启动 NAS Docker Agent", theme->accent, 32, 144, 260);
}

static void draw_home(void)
{
    char value[64];
    const page_theme_t *theme = &PAGE_THEME[PAGE_HOME];
    const char *hostname = safe_text(s_status.nas.hostname, "NAS");
    const char *health = s_online ? health_text(s_status.nas.health) : "离线";
    lv_color_t hc = s_online ? health_color(s_status.nas.health) : lv_color_hex(0xE85D62);

    lv_obj_t *hero = card(s_content, 30, 28, 620, 300, hc, lv_color_hex(0xFFFDF8));
    label(hero, hostname, lv_color_hex(0x1B2826), 34, 44, 360);
    chip(hero, health, hc, 424, 42, 130);
    label(hero, s_online ? "NAS 正在安静运行" : "正在等待 NAS 回应", lv_color_hex(0x61706C), 34, 92, 420);
    format_uptime(s_status.nas.uptime_sec, value, sizeof(value));
    metric(hero, "运行时间", value, 34, 150, 210, theme->accent);
    metric(hero, "主机地址", safe_text(s_status.nas.primary_ip, "--"), 286, 150, 230, lv_color_hex(0x4B95D9));
    snprintf(value, sizeof(value), "%d 条", s_status.nas.alert_count);
    metric(hero, "告警", value, 34, 226, 120, s_status.nas.alert_count > 0 ? lv_color_hex(0xE85D62) : lv_color_hex(0x35A86D));

    lv_obj_t *speed = card(s_content, 680, 28, 314, 300, lv_color_hex(0x36AA69), lv_color_hex(0xF2FFF4));
    char rx[32];
    char tx[32];
    nas_format_bps(s_status.network.total_rx_bps, rx, sizeof(rx));
    nas_format_bps(s_status.network.total_tx_bps, tx, sizeof(tx));
    label(speed, "实时网络", lv_color_hex(0x1B2826), 28, 42, 220);
    metric(speed, "下载", rx, 28, 104, 220, lv_color_hex(0x36AA69));
    metric(speed, "上传", tx, 28, 190, 220, lv_color_hex(0x4B95D9));

    lv_obj_t *cpu = card(s_content, 30, 360, 300, 132, lv_color_hex(0x229B97), lv_color_hex(0xEEFFFA));
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    metric(cpu, "CPU 占用", value, 24, 32, 150, lv_color_hex(0x229B97));
    bar(cpu, 24, 94, 238, pct_i(s_status.cpu.usage_pct), lv_color_hex(0x229B97));

    lv_obj_t *mem = card(s_content, 362, 360, 300, 132, lv_color_hex(0xEF6B5F), lv_color_hex(0xFFF4F0));
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    metric(mem, "内存占用", value, 24, 32, 150, lv_color_hex(0xEF6B5F));
    bar(mem, 24, 94, 238, pct_i(s_status.memory.used_pct), lv_color_hex(0xEF6B5F));

    lv_obj_t *disk = card(s_content, 694, 360, 300, 132, lv_color_hex(0xF1B93F), lv_color_hex(0xFFF8DD));
    storage_summary_t storage = storage_summary();
    if (storage.total_bytes > 0) {
        format_pct(storage.used_pct, value, sizeof(value));
        metric(disk, "总存储已用", value, 24, 32, 150, lv_color_hex(0xF1B93F));
        bar(disk, 24, 94, 238, pct_i(storage.used_pct), lv_color_hex(0xF1B93F));
    } else {
        metric(disk, "总存储已用", "--", 24, 32, 150, lv_color_hex(0xF1B93F));
        bar(disk, 24, 94, 238, 0, lv_color_hex(0xF1B93F));
    }
}

static void draw_perf(void)
{
    char value[64];
    const page_theme_t *theme = &PAGE_THEME[PAGE_PERF];
    lv_obj_t *cpu = card(s_content, 30, 36, 460, 210, theme->accent, lv_color_hex(0xF0FFFA));
    label(cpu, "处理器", lv_color_hex(0x1B2826), 28, 34, 300);
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    metric(cpu, "占用", value, 28, 86, 150, theme->accent);
    format_temp(s_status.cpu.temperature_c, value, sizeof(value));
    metric(cpu, "温度", value, 230, 86, 130, lv_color_hex(0xEF6B5F));
    snprintf(value, sizeof(value), "%.2f / %.2f / %.2f", s_status.cpu.load_one, s_status.cpu.load_five, s_status.cpu.load_fifteen);
    label(cpu, "负载", lv_color_hex(0x65726F), 28, 158, 80);
    label(cpu, value, lv_color_hex(0x1B2826), 106, 158, 290);

    lv_obj_t *mem = card(s_content, 534, 36, 460, 210, lv_color_hex(0xEF6B5F), lv_color_hex(0xFFF4F0));
    label(mem, "内存", lv_color_hex(0x1B2826), 28, 34, 300);
    char used[24];
    char total[24];
    char cache[24];
    nas_format_bytes(s_status.memory.used_bytes, used, sizeof(used));
    nas_format_bytes(s_status.memory.total_bytes, total, sizeof(total));
    nas_format_bytes(s_status.memory.cache_bytes, cache, sizeof(cache));
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    metric(mem, "占用", value, 28, 86, 130, lv_color_hex(0xEF6B5F));
    snprintf(value, sizeof(value), "%s / %s", used, total);
    metric(mem, "已用 / 总量", value, 190, 86, 220, lv_color_hex(0x1B2826));
    label(mem, "缓存", lv_color_hex(0x65726F), 28, 158, 80);
    label(mem, cache, lv_color_hex(0x1B2826), 106, 158, 200);

    lv_obj_t *swap = card(s_content, 30, 286, 964, 168, lv_color_hex(0xF1B93F), lv_color_hex(0xFFF8DD));
    char swap_used[24];
    char swap_total[24];
    nas_format_bytes(s_status.memory.swap_used_bytes, swap_used, sizeof(swap_used));
    nas_format_bytes(s_status.memory.swap_total_bytes, swap_total, sizeof(swap_total));
    format_pct(s_status.memory.swap_used_pct, value, sizeof(value));
    metric(swap, "Swap", value, 28, 44, 140, lv_color_hex(0xF1B93F));
    snprintf(value, sizeof(value), "%s / %s", swap_used, swap_total);
    metric(swap, "已用 / 总量", value, 228, 44, 260, lv_color_hex(0x1B2826));
    chip(swap, health_text(s_status.cpu.health), health_color(s_status.cpu.health), 618, 68, 120);
    chip(swap, health_text(s_status.memory.health), health_color(s_status.memory.health), 762, 68, 120);
}

static void draw_storage(void)
{
    char value[96];
    storage_summary_t summary = storage_summary();
    lv_obj_t *hero = card(s_content, 30, 36, 964, 236, PAGE_THEME[PAGE_STORAGE].accent, lv_color_hex(0xFFF8DD));
    label(hero, "全部存储池", lv_color_hex(0x1B2826), 30, 34, 240);
    if (summary.total_bytes > 0) {
        char total[24];
        char free_b[24];
        char used[24];
        nas_format_bytes(summary.total_bytes, total, sizeof(total));
        nas_format_bytes(summary.free_bytes, free_b, sizeof(free_b));
        nas_format_bytes(summary.used_bytes, used, sizeof(used));
        format_pct(summary.used_pct, value, sizeof(value));
        metric(hero, "已用比例", value, 30, 90, 160, PAGE_THEME[PAGE_STORAGE].accent);
        bar(hero, 230, 118, 600, pct_i(summary.used_pct), PAGE_THEME[PAGE_STORAGE].accent);
        snprintf(value, sizeof(value), "总容量 %s", total);
        metric(hero, "容量", value, 30, 160, 190, lv_color_hex(0x1B2826));
        snprintf(value, sizeof(value), "已用 %s", used);
        metric(hero, "占用", value, 292, 160, 180, lv_color_hex(0xEF6B5F));
        snprintf(value, sizeof(value), "剩余 %s", free_b);
        metric(hero, "可用", value, 540, 160, 180, lv_color_hex(0x36AA69));
        snprintf(value, sizeof(value), "%d 个池", summary.count);
        chip(hero, value, PAGE_THEME[PAGE_STORAGE].accent, 792, 166, 120);
    } else {
        label(hero, "暂无存储池数据", lv_color_hex(0x65726F), 30, 104, 300);
    }

    int pool_cards = s_status.storage.pool_count < 3 ? s_status.storage.pool_count : 3;
    for (int i = 0; i < pool_cards; ++i) {
        const nas_pool_t *pool = &s_status.storage.pools[i];
        int x = 30 + i * 332;
        lv_obj_t *c = card(s_content, x, 314, 300, 160, lv_color_hex(0x708AE8), lv_color_hex(0xF4F7FF));
        label(c, safe_text(pool->name, pool->id), lv_color_hex(0x1B2826), 22, 30, 230);
        format_pct(pool->used_pct, value, sizeof(value));
        metric(c, "已用", value, 22, 74, 120, lv_color_hex(0x708AE8));
        snprintf(value, sizeof(value), "%s / %s", safe_text(pool->raid_type, "RAID"), health_text(pool->raid_status));
        chip(c, value, health_color(pool->health), 142, 74, 120);
        bar(c, 22, 126, 230, pct_i(pool->used_pct), lv_color_hex(0x708AE8));
    }

    if (pool_cards == 0) {
        for (int i = 0; i < s_status.storage.volume_count && i < 3; ++i) {
            const nas_volume_t *vol = &s_status.storage.volumes[i];
            int x = 30 + i * 332;
            lv_obj_t *c = card(s_content, x, 314, 300, 160, lv_color_hex(0x708AE8), lv_color_hex(0xF4F7FF));
            label(c, safe_text(vol->name, vol->id), lv_color_hex(0x1B2826), 22, 30, 230);
            format_pct(vol->used_pct, value, sizeof(value));
            metric(c, "已用", value, 22, 74, 120, lv_color_hex(0x708AE8));
            bar(c, 22, 126, 230, pct_i(vol->used_pct), lv_color_hex(0x708AE8));
        }
    }
}

static void draw_drives(void)
{
    if (s_status.drive_count == 0 && s_status.nvme_count == 0) {
        draw_empty("暂无硬盘或 M.2 数据。");
        return;
    }

    char value[64];
    for (int i = 0; i < s_status.drive_count && i < 6; ++i) {
        const nas_drive_t *drive = &s_status.drives[i];
        int x = 30 + (i % 3) * 332;
        int y = 30 + (i / 3) * 208;
        lv_color_t hc = health_color(drive->health);
        lv_obj_t *c = card(s_content, x, y, 300, 176, hc, lv_color_hex(0xF8FBFF));
        label(c, safe_text(drive->bay, drive->id), lv_color_hex(0x1B2826), 22, 30, 120);
        chip(c, health_text(drive->smart_status), hc, 166, 28, 96);
        format_temp(drive->temperature_c, value, sizeof(value));
        metric(c, "温度", value, 22, 78, 96, lv_color_hex(0xEF6B5F));
        snprintf(value, sizeof(value), "%d", drive->bad_sector_count);
        metric(c, "坏道", value, 138, 78, 80, hc);
        snprintf(value, sizeof(value), "%d小时", drive->power_on_hours);
        label(c, value, lv_color_hex(0x65726F), 22, 142, 180);
    }

    for (int i = 0; i < s_status.nvme_count && i < 3; ++i) {
        const nas_nvme_t *nvme = &s_status.nvme[i];
        int x = 30 + i * 332;
        lv_obj_t *c = card(s_content, x, 446, 300, 74, lv_color_hex(0xDD73A8), lv_color_hex(0xFFF3FA));
        snprintf(value, sizeof(value), "%s · %d%%磨损", safe_text(nvme->slot, "M.2"), nvme->percentage_used_pct);
        label(c, value, lv_color_hex(0x1B2826), 22, 28, 220);
    }
}

static void draw_network(void)
{
    char rx[32];
    char tx[32];
    char value[64];
    nas_format_bps(s_status.network.total_rx_bps, rx, sizeof(rx));
    nas_format_bps(s_status.network.total_tx_bps, tx, sizeof(tx));

    lv_obj_t *traffic = card(s_content, 30, 34, 410, 188, PAGE_THEME[PAGE_NETWORK].accent, lv_color_hex(0xF0FFF3));
    label(traffic, "实时流量", lv_color_hex(0x1B2826), 30, 32, 160);
    metric(traffic, "下载", rx, 30, 86, 160, PAGE_THEME[PAGE_NETWORK].accent);
    metric(traffic, "上传", tx, 218, 86, 160, lv_color_hex(0x4B95D9));
    mark(traffic, 30, 154, 318, 6, PAGE_THEME[PAGE_NETWORK].accent, LV_OPA_40);

    lv_obj_t *summary = card(s_content, 472, 34, 248, 188, lv_color_hex(0xF1B93F), lv_color_hex(0xFFF8DD));
    label(summary, "网口概览", lv_color_hex(0x1B2826), 30, 32, 160);
    snprintf(value, sizeof(value), "%d / %d", connected_interface_count(), s_status.network.interface_count);
    metric(summary, "接入 / 总数", value, 30, 86, 160, lv_color_hex(0xF1B93F));

    lv_obj_t *host = card(s_content, 752, 34, 242, 188, lv_color_hex(0x4B95D9), lv_color_hex(0xF0F8FF));
    label(host, "主机入口", lv_color_hex(0x1B2826), 30, 32, 160);
    label(host, safe_text(s_status.nas.primary_ip, "暂无主机 IP"), lv_color_hex(0x1B2826), 30, 94, 172);
    chip(host, health_text(s_status.network.health), health_color(s_status.network.health), 30, 138, 100);

    for (int i = 0; i < s_status.network.interface_count && i < 6; ++i) {
        const nas_interface_t *iface = &s_status.network.interfaces[i];
        int x = 30 + (i % 3) * 332;
        int y = 252 + (i / 3) * 138;
        lv_color_t state_color = health_color(iface->status);
        lv_obj_t *c = card(s_content, x, y, 300, 118, state_color, lv_color_hex(0xFFFDF8));
        label(c, safe_text(iface->name, "网口"), lv_color_hex(0x1B2826), 22, 20, 92);
        chip(c, state_text(iface->status), state_color, 116, 16, 80);
        format_link_speed(iface, rx, sizeof(rx));
        chip(c, rx, PAGE_THEME[PAGE_NETWORK].accent, 204, 16, 74);
        snprintf(value, sizeof(value), "IP %s", interface_ip(iface, i));
        label(c, value, lv_color_hex(0x1B2826), 22, 56, 240);
        snprintf(rx, sizeof(rx), "错 %d/%d  丢 %d/%d", iface->rx_errors, iface->tx_errors, iface->rx_dropped, iface->tx_dropped);
        label(c, rx, lv_color_hex(0x65726F), 22, 88, 240);
    }
}

static void draw_services(void)
{
    char value[64];
    int total = s_status.workloads.docker.running + s_status.workloads.docker.stopped;
    if (total < s_status.workloads.docker.container_count) {
        total = s_status.workloads.docker.container_count;
    }

    lv_obj_t *docker = card(s_content, 30, 34, 964, 156, PAGE_THEME[PAGE_SERVICES].accent, lv_color_hex(0xF6F0FF));
    label(docker, "Docker 容器", lv_color_hex(0x1B2826), 30, 30, 220);
    snprintf(value, sizeof(value), "%d", total);
    metric(docker, "总数", value, 280, 42, 110, PAGE_THEME[PAGE_SERVICES].accent);
    snprintf(value, sizeof(value), "%d", s_status.workloads.docker.running);
    metric(docker, "运行", value, 438, 42, 110, lv_color_hex(0x35A86D));
    snprintf(value, sizeof(value), "%d", s_status.workloads.docker.stopped);
    metric(docker, "停止", value, 596, 42, 110, lv_color_hex(0x87928F));
    snprintf(value, sizeof(value), "%d", s_status.workloads.docker.unhealthy);
    metric(docker, "异常", value, 754, 42, 110, s_status.workloads.docker.unhealthy > 0 ? lv_color_hex(0xE85D62) : lv_color_hex(0x35A86D));
    label(docker, "仅展示 Docker 信息，容器状态由 NAS Agent 只读采集。", lv_color_hex(0x65726F), 30, 106, 680);
    mark(docker, 30, 132, 862, 6, PAGE_THEME[PAGE_SERVICES].accent, LV_OPA_30);

    if (s_status.workloads.docker.container_count == 0) {
        lv_obj_t *empty = card(s_content, 252, 260, 520, 160, PAGE_THEME[PAGE_SERVICES].accent, lv_color_hex(0xFFFDF8));
        label(empty, "暂无容器详情", lv_color_hex(0x1B2826), 30, 42, 260);
        label(empty, "请确认 NAS Agent 可以读取 Docker socket。", lv_color_hex(0x65726F), 30, 92, 380);
        return;
    }

    for (int i = 0; i < s_status.workloads.docker.container_count && i < 6; ++i) {
        const nas_container_t *container = &s_status.workloads.docker.containers[i];
        int x = 30 + (i % 2) * 492;
        int y = 226 + (i / 2) * 96;
        lv_color_t hc = health_color(container->health);
        lv_obj_t *c = card(s_content, x, y, 460, 78, hc, lv_color_hex(0xFFFDF8));
        label(c, safe_text(container->name, "容器"), lv_color_hex(0x1B2826), 22, 24, 250);
        chip(c, state_text(container->state), hc, 292, 18, 82);
        snprintf(value, sizeof(value), "健康 %s", health_text(container->health));
        label(c, value, lv_color_hex(0x65726F), 22, 52, 180);
    }
}

static void draw_settings(void)
{
    char endpoint[96];
    snprintf(endpoint, sizeof(endpoint), "%s:%d", s_endpoint_host, s_endpoint_port);

    lv_obj_t *hero = card(s_content, 120, 56, 784, 230, PAGE_THEME[PAGE_SETTINGS].accent, lv_color_hex(0xF2FBFF));
    label(hero, "Web 后台", lv_color_hex(0x1B2826), 38, 40, 240);
    label(hero, s_web_url, PAGE_THEME[PAGE_SETTINGS].accent, 38, 96, 560);
    label(hero, "用电脑或手机浏览器打开这个地址，配置 NAS 地址、Token 和刷新间隔。", lv_color_hex(0x65726F), 38, 154, 650);

    lv_obj_t *info = card(s_content, 120, 334, 784, 150, lv_color_hex(0x35A86D), lv_color_hex(0xF4FFF7));
    metric(info, "当前 NAS Agent", endpoint, 38, 38, 300, lv_color_hex(0x1B2826));
    chip(info, s_online ? "在线" : "离线", s_online ? lv_color_hex(0x35A86D) : lv_color_hex(0xE85D62), 560, 58, 120);
}

static void clear_content(void)
{
    lv_obj_clean(s_content);
}

static void draw_page(void)
{
    clear_content();
    draw_page_background();
    draw_background_marks(PAGE_THEME[s_page].accent);

    if (!s_has_status && s_page != PAGE_SETTINGS) {
        draw_empty("设备还没有收到 NAS 状态。");
        return;
    }

    switch (s_page) {
    case PAGE_HOME:
        draw_home();
        break;
    case PAGE_PERF:
        draw_perf();
        break;
    case PAGE_STORAGE:
        draw_storage();
        break;
    case PAGE_DRIVES:
        draw_drives();
        break;
    case PAGE_NETWORK:
        draw_network();
        break;
    case PAGE_SERVICES:
        draw_services();
        break;
    case PAGE_SETTINGS:
        draw_settings();
        break;
    default:
        break;
    }
}

static void refresh_header(void)
{
    const page_theme_t *theme = &PAGE_THEME[s_page];
    lv_obj_set_style_bg_color(s_header, lv_color_mix(theme->accent, lv_color_white(), 44), 0);
    lv_obj_set_style_border_color(s_header, theme->accent, 0);
    lv_label_set_text(s_title_label, theme->name);
    lv_label_set_text(s_hint_label, s_message);
    lv_label_set_text(s_online_badge, s_online ? "在线" : "离线");
    lv_obj_set_style_bg_color(s_online_badge, s_online ? lv_color_hex(0x35A86D) : lv_color_hex(0xE85D62), 0);

    char page[32];
    snprintf(page, sizeof(page), "%d/%d", (int)s_page + 1, PAGE_COUNT);
    lv_label_set_text(s_page_label, page);
    for (int i = 0; i < PAGE_COUNT; ++i) {
        lv_obj_set_style_bg_color(s_page_dots[i], i == s_page ? theme->accent : lv_color_hex(0xD7CCC0), 0);
        lv_obj_set_style_border_color(s_page_dots[i], i == s_page ? lv_color_hex(0x1B2826) : lv_color_hex(0xD7CCC0), 0);
    }
}

static void set_page(ui_page_t page)
{
    if (page < 0 || page >= PAGE_COUNT || page == s_page) {
        return;
    }
    s_page = page;
    refresh_header();
    draw_page();
}

static void gesture_event_cb(lv_event_t *event)
{
    (void)event;
    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) {
        return;
    }

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_LEFT) {
        set_page((ui_page_t)((s_page + 1) % PAGE_COUNT));
    } else if (dir == LV_DIR_RIGHT) {
        set_page((ui_page_t)((s_page + PAGE_COUNT - 1) % PAGE_COUNT));
    }
}

static void init_styles(void)
{
    lv_style_init(&s_style_screen);
    lv_style_set_bg_color(&s_style_screen, lv_color_hex(0xFFF4E3));
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);

    lv_style_init(&s_style_header);
    lv_style_set_bg_opa(&s_style_header, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_header, 0);
    lv_style_set_border_side(&s_style_header, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_pad_all(&s_style_header, 0);

    lv_style_init(&s_style_card);
    lv_style_set_bg_opa(&s_style_card, LV_OPA_90);
    lv_style_set_radius(&s_style_card, 8);
    lv_style_set_border_width(&s_style_card, 3);
    lv_style_set_pad_all(&s_style_card, 0);
    lv_style_set_shadow_width(&s_style_card, 8);
    lv_style_set_shadow_ofs_x(&s_style_card, 4);
    lv_style_set_shadow_ofs_y(&s_style_card, 5);
    lv_style_set_shadow_color(&s_style_card, lv_color_hex(0xD8C8B7));

    lv_style_init(&s_style_chip);
    lv_style_set_bg_opa(&s_style_chip, LV_OPA_COVER);
    lv_style_set_radius(&s_style_chip, 8);
    lv_style_set_border_width(&s_style_chip, 2);
    lv_style_set_pad_all(&s_style_chip, 0);
}

void ui_set_endpoint_config(const char *host, int port)
{
    if (host != NULL && host[0] != '\0') {
        strlcpy(s_endpoint_host, host, sizeof(s_endpoint_host));
    }
    if (port > 0 && port <= 65535) {
        s_endpoint_port = port;
    }
}

void ui_set_endpoint_save_callback(ui_endpoint_save_cb_t callback, void *user_data)
{
    s_save_cb = callback;
    s_save_user_data = user_data;
    (void)s_save_cb;
    (void)s_save_user_data;
}

void ui_set_web_url(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return;
    }
    strlcpy(s_web_url, url, sizeof(s_web_url));
}

void ui_init(void)
{
    if (!board_lvgl_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }

    init_styles();
    nas_status_init(&s_status);

    lv_obj_t *screen = lv_scr_act();
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, &s_style_screen, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, gesture_event_cb, LV_EVENT_GESTURE, NULL);

    s_header = lv_obj_create(screen);
    lv_obj_add_style(s_header, &s_style_header, 0);
    lv_obj_set_pos(s_header, 0, 0);
    lv_obj_set_size(s_header, SCREEN_W, HEADER_H);
    lv_obj_add_flag(s_header, LV_OBJ_FLAG_GESTURE_BUBBLE);

    s_title_label = label(s_header, PAGE_THEME[s_page].name, lv_color_hex(0x1B2826), 28, 18, 120);
    s_hint_label = label(s_header, "", lv_color_hex(0x4E605C), 170, 18, 520);

    s_online_badge = lv_label_create(s_header);
    lv_obj_add_style(s_online_badge, &s_style_chip, 0);
    lv_obj_set_style_text_font(s_online_badge, FONT_CN, 0);
    lv_obj_set_style_text_color(s_online_badge, lv_color_white(), 0);
    lv_label_set_text(s_online_badge, "离线");
    lv_obj_set_style_bg_color(s_online_badge, lv_color_hex(0xE85D62), 0);
    lv_obj_set_style_border_color(s_online_badge, lv_color_hex(0x1B2826), 0);
    lv_obj_set_pos(s_online_badge, 784, 16);
    lv_obj_set_size(s_online_badge, 92, 36);
    lv_obj_set_style_text_align(s_online_badge, LV_TEXT_ALIGN_CENTER, 0);

    s_page_label = label(s_header, "", lv_color_hex(0x1B2826), 910, 21, 80);
    lv_obj_set_style_text_align(s_page_label, LV_TEXT_ALIGN_RIGHT, 0);

    for (int i = 0; i < PAGE_COUNT; ++i) {
        s_page_dots[i] = lv_obj_create(s_header);
        lv_obj_remove_style_all(s_page_dots[i]);
        lv_obj_set_pos(s_page_dots[i], 888 + i * 14, 48);
        lv_obj_set_size(s_page_dots[i], 8, 8);
        lv_obj_set_style_radius(s_page_dots[i], 4, 0);
        lv_obj_set_style_bg_opa(s_page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_page_dots[i], 1, 0);
    }

    s_content = lv_obj_create(screen);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_pos(s_content, 0, HEADER_H);
    lv_obj_set_size(s_content, SCREEN_W, CONTENT_H);
    lv_obj_set_style_bg_color(s_content, PAGE_THEME[s_page].bg, 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_content, LV_OBJ_FLAG_GESTURE_BUBBLE);

    refresh_header();
    draw_page();
    board_lvgl_unlock();
}

void ui_set_message(const char *message)
{
    if (!board_lvgl_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }
    strlcpy(s_message, safe_text(message, ""), sizeof(s_message));
    refresh_header();
    if (!s_has_status || s_page == PAGE_SETTINGS) {
        draw_page();
    }
    board_lvgl_unlock();
}

void ui_update_status(const nas_status_t *status, bool online)
{
    if (status == NULL) {
        return;
    }

    if (!board_lvgl_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }
    s_status = *status;
    s_has_status = true;
    s_online = online;
    strlcpy(s_message, online ? "数据已刷新" : "NAS 暂时离线", sizeof(s_message));
    refresh_header();
    draw_page();
    board_lvgl_unlock();
}
