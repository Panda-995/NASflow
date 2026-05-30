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
LV_FONT_DECLARE(lv_font_nas_cn_26);
#define FONT_CN (&lv_font_nas_cn_18)
#define FONT_BIG (&lv_font_nas_cn_26)

#define SCREEN_W 1024
#define SCREEN_H 600
#define HEADER_H 66
#define CONTENT_H (SCREEN_H - HEADER_H)
#define UI_LOCK_TIMEOUT_MS 1000
#define UI_HISTORY_POINTS 18

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
static lv_obj_t *s_online_text;
static lv_obj_t *s_power_badge;
static lv_obj_t *s_power_text;
static lv_obj_t *s_page_label;
static lv_obj_t *s_page_dots[PAGE_COUNT];

static ui_page_t s_page = PAGE_HOME;
static nas_status_t s_status;
static bool s_has_status;
static bool s_online;
static power_status_t s_power_status = {
    .battery_pct = -1,
    .voltage_mv = -1,
    .source = POWER_SUPPLY_UNKNOWN,
    .charge_state = POWER_CHARGE_UNKNOWN,
};
static char s_message[96] = "正在启动";
static char s_endpoint_host[64] = CONFIG_NAS_DISPLAY_API_HOST;
static int s_endpoint_port = CONFIG_NAS_DISPLAY_API_PORT;
static char s_web_url[64] = "连接 Wi-Fi 后显示";
static ui_endpoint_save_cb_t s_save_cb;
static void *s_save_user_data;
static uint8_t s_cpu_history[UI_HISTORY_POINTS];
static uint8_t s_mem_history[UI_HISTORY_POINTS];
static uint8_t s_storage_history[UI_HISTORY_POINTS];
static uint8_t s_net_history[UI_HISTORY_POINTS];
static int s_history_len;
static uint64_t s_net_peak_bps;

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

static lv_color_t power_color(void)
{
    if (s_power_status.charge_state == POWER_CHARGE_CHARGING ||
        s_power_status.charge_state == POWER_CHARGE_FULL) {
        return lv_color_hex(0x35A86D);
    }
    if (s_power_status.battery_pct >= 0 && s_power_status.battery_pct <= 20) {
        return lv_color_hex(0xE85D62);
    }
    if (s_power_status.source == POWER_SUPPLY_BATTERY) {
        return lv_color_hex(0xF1B93F);
    }
    return lv_color_hex(0x4B95D9);
}

static const char *power_source_text(power_supply_t source)
{
    switch (source) {
    case POWER_SUPPLY_USB:
        return "USB";
    case POWER_SUPPLY_BATTERY:
        return "电池";
    case POWER_SUPPLY_UNKNOWN:
    default:
        return "供电";
    }
}

static const char *charge_text(power_charge_state_t state)
{
    switch (state) {
    case POWER_CHARGE_CHARGING:
        return "充电";
    case POWER_CHARGE_FULL:
        return "已满";
    case POWER_CHARGE_DISCHARGING:
        return "放电";
    case POWER_CHARGE_NOT_CHARGING:
        return "未充";
    case POWER_CHARGE_UNKNOWN:
    default:
        return "未知";
    }
}

static void format_power_status(char *out, size_t out_size)
{
    const char *source = power_source_text(s_power_status.source);
    const char *charge = charge_text(s_power_status.charge_state);
    if (s_power_status.battery_pct >= 0) {
        snprintf(out, out_size, "%s %d%% %s", source, s_power_status.battery_pct, charge);
    } else {
        snprintf(out, out_size, "电量 -- %s", charge);
    }
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

static void push_history(void)
{
    int idx;
    if (s_history_len < UI_HISTORY_POINTS) {
        idx = s_history_len++;
    } else {
        memmove(s_cpu_history, s_cpu_history + 1, UI_HISTORY_POINTS - 1);
        memmove(s_mem_history, s_mem_history + 1, UI_HISTORY_POINTS - 1);
        memmove(s_storage_history, s_storage_history + 1, UI_HISTORY_POINTS - 1);
        memmove(s_net_history, s_net_history + 1, UI_HISTORY_POINTS - 1);
        idx = UI_HISTORY_POINTS - 1;
    }

    storage_summary_t storage = storage_summary();
    s_cpu_history[idx] = (uint8_t)pct_i(s_status.cpu.usage_pct);
    s_mem_history[idx] = (uint8_t)pct_i(s_status.memory.used_pct);
    s_storage_history[idx] = (uint8_t)pct_i(storage.used_pct);

    uint64_t net_bps = s_status.network.total_rx_bps + s_status.network.total_tx_bps;
    if (net_bps > s_net_peak_bps) {
        s_net_peak_bps = net_bps;
    } else if (s_net_peak_bps > 0) {
        s_net_peak_bps = (s_net_peak_bps * 15 + net_bps) / 16;
    }
    int net_pct = s_net_peak_bps > 0 ? (int)((net_bps * 100) / s_net_peak_bps) : 0;
    s_net_history[idx] = (uint8_t)pct_i((float)net_pct);
}

static lv_obj_t *label_font(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color, int x, int y, int w)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, font, 0);
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

static lv_obj_t *label(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w)
{
    return label_font(parent, text, FONT_CN, color, x, y, w);
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

static void mini_wave(lv_obj_t *parent, const uint8_t *values, int count, int x, int y, int w, int h, lv_color_t accent)
{
    const int gap = 4;
    int bar_w = (w - (UI_HISTORY_POINTS - 1) * gap) / UI_HISTORY_POINTS;
    if (bar_w < 4) {
        bar_w = 4;
    }
    int points = count < UI_HISTORY_POINTS ? count : UI_HISTORY_POINTS;
    for (int i = 0; i < UI_HISTORY_POINTS; ++i) {
        int src = i - (UI_HISTORY_POINTS - points);
        int v = src >= 0 ? values[src] : 0;
        int bh = 8 + (h - 8) * v / 100;
        lv_color_t color = lv_color_mix(accent, lv_color_white(), 64 + (i % 3) * 24);
        mark(parent, x + i * (bar_w + gap), y + h - bh, bar_w, bh, color, src >= 0 ? LV_OPA_80 : LV_OPA_20);
    }
}

static void donut(lv_obj_t *parent, int x, int y, int size, int value, lv_color_t accent, const char *center)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_pos(arc, x, y);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_value(arc, value);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xF2E3D0), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);

    lv_obj_t *txt = label_font(parent, center, FONT_BIG, accent, x, y + size / 2 - 16, size);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
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
    lv_obj_set_style_bg_opa(wash, LV_OPA_10, 0);
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

static lv_obj_t *status_badge(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w,
                              lv_obj_t **text_obj)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_add_style(obj, &s_style_chip, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x1B2826), 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, 40);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *txt = label(obj, text, lv_color_white(), 0, 0, w);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(txt);
    if (text_obj != NULL) {
        *text_obj = txt;
    }
    return obj;
}

static void metric(lv_obj_t *parent, const char *name, const char *value, int x, int y, int w, lv_color_t accent)
{
    label(parent, name, lv_color_hex(0x65726F), x, y, w);
    lv_obj_t *v = label(parent, value, lv_color_hex(0x1C2927), x, y + 32, w);
    lv_obj_set_style_text_color(v, accent, 0);
}

static void metric_big(lv_obj_t *parent, const char *name, const char *value, int x, int y, int w, lv_color_t accent)
{
    label(parent, name, lv_color_hex(0x65726F), x, y, w);
    lv_obj_t *v = label_font(parent, value, FONT_BIG, accent, x, y + 28, w);
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
    label_font(hero, hostname, FONT_BIG, lv_color_hex(0x1B2826), 34, 38, 360);
    chip(hero, health, hc, 424, 42, 130);
    label(hero, s_online ? "NAS 正在安静运行" : "正在等待 NAS 回应", lv_color_hex(0x61706C), 34, 92, 420);
    format_uptime(s_status.nas.uptime_sec, value, sizeof(value));
    metric_big(hero, "运行时间", value, 34, 148, 210, theme->accent);
    metric(hero, "主机地址", safe_text(s_status.nas.primary_ip, "--"), 286, 150, 230, lv_color_hex(0x4B95D9));
    format_temp(s_status.cpu.temperature_c, value, sizeof(value));
    metric_big(hero, "设备温度", value, 34, 226, 150, lv_color_hex(0xEF6B5F));
    snprintf(value, sizeof(value), "%d 条", s_status.nas.alert_count);
    metric(hero, "告警", value, 286, 226, 120, s_status.nas.alert_count > 0 ? lv_color_hex(0xE85D62) : lv_color_hex(0x35A86D));

    lv_obj_t *speed = card(s_content, 680, 28, 314, 300, lv_color_hex(0x36AA69), lv_color_hex(0xF2FFF4));
    char rx[32];
    char tx[32];
    nas_format_bps(s_status.network.total_rx_bps, rx, sizeof(rx));
    nas_format_bps(s_status.network.total_tx_bps, tx, sizeof(tx));
    label(speed, "实时网络", lv_color_hex(0x1B2826), 28, 42, 220);
    metric_big(speed, "下载", rx, 28, 92, 220, lv_color_hex(0x36AA69));
    metric(speed, "上传", tx, 28, 178, 220, lv_color_hex(0x4B95D9));
    mini_wave(speed, s_net_history, s_history_len, 28, 246, 238, 34, lv_color_hex(0x36AA69));

    lv_obj_t *cpu = card(s_content, 30, 360, 300, 132, lv_color_hex(0x229B97), lv_color_hex(0xEEFFFA));
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    metric_big(cpu, "CPU 占用", value, 24, 26, 120, lv_color_hex(0x229B97));
    mini_wave(cpu, s_cpu_history, s_history_len, 134, 32, 128, 48, lv_color_hex(0x229B97));
    bar(cpu, 24, 100, 238, pct_i(s_status.cpu.usage_pct), lv_color_hex(0x229B97));

    lv_obj_t *mem = card(s_content, 362, 360, 300, 132, lv_color_hex(0xEF6B5F), lv_color_hex(0xFFF4F0));
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    metric_big(mem, "内存占用", value, 24, 26, 120, lv_color_hex(0xEF6B5F));
    mini_wave(mem, s_mem_history, s_history_len, 134, 32, 128, 48, lv_color_hex(0xEF6B5F));
    bar(mem, 24, 100, 238, pct_i(s_status.memory.used_pct), lv_color_hex(0xEF6B5F));

    lv_obj_t *disk = card(s_content, 694, 360, 300, 132, lv_color_hex(0xF1B93F), lv_color_hex(0xFFF8DD));
    storage_summary_t storage = storage_summary();
    if (storage.total_bytes > 0) {
        format_pct(storage.used_pct, value, sizeof(value));
        metric_big(disk, "总存储已用", value, 24, 26, 136, lv_color_hex(0xF1B93F));
        donut(disk, 184, 24, 76, pct_i(storage.used_pct), lv_color_hex(0xF1B93F), value);
        bar(disk, 24, 100, 238, pct_i(storage.used_pct), lv_color_hex(0xF1B93F));
    } else {
        metric_big(disk, "总存储已用", "--", 24, 26, 136, lv_color_hex(0xF1B93F));
        donut(disk, 184, 24, 76, 0, lv_color_hex(0xF1B93F), "--");
        bar(disk, 24, 100, 238, 0, lv_color_hex(0xF1B93F));
    }
}

static void draw_perf(void)
{
    char value[64];
    const page_theme_t *theme = &PAGE_THEME[PAGE_PERF];
    lv_obj_t *cpu = card(s_content, 30, 36, 460, 210, theme->accent, lv_color_hex(0xF0FFFA));
    label(cpu, "处理器", lv_color_hex(0x1B2826), 28, 34, 300);
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    metric_big(cpu, "占用", value, 28, 78, 130, theme->accent);
    format_temp(s_status.cpu.temperature_c, value, sizeof(value));
    metric_big(cpu, "温度", value, 170, 78, 120, lv_color_hex(0xEF6B5F));
    snprintf(value, sizeof(value), "%.1f / %.1f / %.1f", s_status.cpu.load_one, s_status.cpu.load_five, s_status.cpu.load_fifteen);
    label(cpu, "负载", lv_color_hex(0x65726F), 310, 86, 80);
    label(cpu, value, lv_color_hex(0x1B2826), 310, 118, 116);
    mini_wave(cpu, s_cpu_history, s_history_len, 28, 156, 372, 34, theme->accent);

    lv_obj_t *mem = card(s_content, 534, 36, 460, 210, lv_color_hex(0xEF6B5F), lv_color_hex(0xFFF4F0));
    label(mem, "内存", lv_color_hex(0x1B2826), 28, 34, 300);
    char used[24];
    char total[24];
    char cache[24];
    nas_format_bytes(s_status.memory.used_bytes, used, sizeof(used));
    nas_format_bytes(s_status.memory.total_bytes, total, sizeof(total));
    nas_format_bytes(s_status.memory.cache_bytes, cache, sizeof(cache));
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    metric_big(mem, "占用", value, 28, 78, 130, lv_color_hex(0xEF6B5F));
    snprintf(value, sizeof(value), "%s / %s", used, total);
    metric(mem, "已用 / 总量", value, 190, 82, 220, lv_color_hex(0x1B2826));
    label(mem, "缓存", lv_color_hex(0x65726F), 28, 158, 80);
    label(mem, cache, lv_color_hex(0x1B2826), 106, 158, 120);
    mini_wave(mem, s_mem_history, s_history_len, 238, 150, 166, 40, lv_color_hex(0xEF6B5F));

    lv_obj_t *swap = card(s_content, 30, 286, 964, 168, lv_color_hex(0xF1B93F), lv_color_hex(0xFFF8DD));
    char swap_used[24];
    char swap_total[24];
    nas_format_bytes(s_status.memory.swap_used_bytes, swap_used, sizeof(swap_used));
    nas_format_bytes(s_status.memory.swap_total_bytes, swap_total, sizeof(swap_total));
    format_pct(s_status.memory.swap_used_pct, value, sizeof(value));
    metric_big(swap, "Swap", value, 28, 36, 140, lv_color_hex(0xF1B93F));
    snprintf(value, sizeof(value), "%s / %s", swap_used, swap_total);
    metric(swap, "已用 / 总量", value, 228, 40, 260, lv_color_hex(0x1B2826));
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
        metric_big(hero, "已用比例", value, 30, 82, 160, PAGE_THEME[PAGE_STORAGE].accent);
        bar(hero, 230, 116, 500, pct_i(summary.used_pct), PAGE_THEME[PAGE_STORAGE].accent);
        mini_wave(hero, s_storage_history, s_history_len, 230, 72, 500, 30, PAGE_THEME[PAGE_STORAGE].accent);
        donut(hero, 792, 44, 110, pct_i(summary.used_pct), PAGE_THEME[PAGE_STORAGE].accent, value);
        snprintf(value, sizeof(value), "总容量 %s", total);
        metric(hero, "容量", value, 30, 160, 190, lv_color_hex(0x1B2826));
        snprintf(value, sizeof(value), "已用 %s", used);
        metric(hero, "占用", value, 292, 160, 180, lv_color_hex(0xEF6B5F));
        snprintf(value, sizeof(value), "剩余 %s", free_b);
        metric(hero, "可用", value, 540, 160, 180, lv_color_hex(0x36AA69));
        snprintf(value, sizeof(value), "%d 个池", summary.count);
        chip(hero, value, PAGE_THEME[PAGE_STORAGE].accent, 788, 174, 120);
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
    metric_big(traffic, "下载", rx, 30, 76, 160, PAGE_THEME[PAGE_NETWORK].accent);
    metric_big(traffic, "上传", tx, 218, 76, 160, lv_color_hex(0x4B95D9));
    mini_wave(traffic, s_net_history, s_history_len, 30, 142, 318, 28, PAGE_THEME[PAGE_NETWORK].accent);

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

    lv_obj_t *docker = card(s_content, 30, 30, 964, 152, PAGE_THEME[PAGE_SERVICES].accent, lv_color_hex(0xF6F0FF));
    label(docker, "Docker 容器", lv_color_hex(0x1B2826), 30, 30, 220);
    snprintf(value, sizeof(value), "%d", total);
    metric_big(docker, "总数", value, 280, 34, 110, PAGE_THEME[PAGE_SERVICES].accent);
    snprintf(value, sizeof(value), "%d", s_status.workloads.docker.running);
    metric_big(docker, "运行", value, 438, 34, 110, lv_color_hex(0x35A86D));
    snprintf(value, sizeof(value), "%d", s_status.workloads.docker.stopped);
    metric_big(docker, "停止", value, 596, 34, 110, lv_color_hex(0x87928F));
    int running_pct = total > 0 ? (s_status.workloads.docker.running * 100) / total : 0;
    snprintf(value, sizeof(value), "%d%%", running_pct);
    donut(docker, 790, 22, 92, running_pct, lv_color_hex(0x35A86D), value);
    label(docker, "运行占比", lv_color_hex(0x65726F), 784, 112, 120);
    label(docker, "Docker 信息只读采集，容器列表可上下滑动。", lv_color_hex(0x65726F), 30, 108, 680);

    label(s_content, "容器列表", lv_color_hex(0x1B2826), 48, 194, 160);
    label(s_content, "上下滑动查看更多", lv_color_hex(0x65726F), 180, 194, 220);
    lv_obj_t *list = lv_obj_create(s_content);
    lv_obj_remove_style_all(list);
    lv_obj_set_pos(list, 30, 224);
    lv_obj_set_size(list, 964, 298);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xFFFDF8), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_90, 0);
    lv_obj_set_style_border_width(list, 2, 0);
    lv_obj_set_style_border_color(list, lv_color_mix(PAGE_THEME[PAGE_SERVICES].accent, lv_color_white(), 100), 0);
    lv_obj_set_style_radius(list, 8, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_shadow_width(list, 6, 0);
    lv_obj_set_style_shadow_ofs_x(list, 3, 0);
    lv_obj_set_style_shadow_ofs_y(list, 4, 0);
    lv_obj_set_style_shadow_color(list, lv_color_hex(0xD8C8B7), 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(list, PAGE_THEME[PAGE_SERVICES].accent, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_60, LV_PART_SCROLLBAR);
    lv_obj_add_flag(list, LV_OBJ_FLAG_GESTURE_BUBBLE);

    if (s_status.workloads.docker.container_count == 0) {
        lv_obj_t *empty = lv_obj_create(list);
        lv_obj_remove_style_all(empty);
        lv_obj_set_pos(empty, 222, 58);
        lv_obj_set_size(empty, 520, 160);
        lv_obj_set_style_bg_color(empty, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(empty, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(empty, 2, 0);
        lv_obj_set_style_border_color(empty, PAGE_THEME[PAGE_SERVICES].accent, 0);
        lv_obj_set_style_radius(empty, 8, 0);
        lv_obj_clear_flag(empty, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(empty, LV_OBJ_FLAG_GESTURE_BUBBLE);
        label(empty, "暂无容器详情", lv_color_hex(0x1B2826), 30, 42, 260);
        label(empty, "请确认 NAS Agent 可以读取 Docker socket。", lv_color_hex(0x65726F), 30, 92, 380);
        return;
    }

    for (int i = 0; i < s_status.workloads.docker.container_count; ++i) {
        const nas_container_t *container = &s_status.workloads.docker.containers[i];
        bool running = text_equals(container->state, "running") || text_equals(container->state, "up") ||
                       text_equals(container->state, "online") || text_equals(container->state, "active");
        bool known = running || text_equals(container->state, "stopped") || text_equals(container->state, "exited") ||
                     text_equals(container->state, "down");
        lv_color_t state_color = running ? lv_color_hex(0x35A86D) : lv_color_hex(0x87928F);
        int y = 14 + i * 74;
        lv_obj_t *c = lv_obj_create(list);
        lv_obj_remove_style_all(c);
        lv_obj_set_pos(c, 18, y);
        lv_obj_set_size(c, 908, 62);
        lv_obj_set_style_bg_color(c, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 2, 0);
        lv_obj_set_style_border_color(c, lv_color_mix(state_color, lv_color_white(), 80), 0);
        lv_obj_set_style_radius(c, 8, 0);
        lv_obj_set_style_shadow_width(c, 4, 0);
        lv_obj_set_style_shadow_ofs_x(c, 2, 0);
        lv_obj_set_style_shadow_ofs_y(c, 2, 0);
        lv_obj_set_style_shadow_color(c, lv_color_hex(0xE2D8CF), 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_GESTURE_BUBBLE);
        mark(c, 0, 0, 8, 62, state_color, LV_OPA_COVER);
        snprintf(value, sizeof(value), "#%02d", i + 1);
        label(c, value, lv_color_hex(0x65726F), 24, 20, 48);
        label(c, safe_text(container->name, "容器"), lv_color_hex(0x1B2826), 86, 19, 560);
        chip(c, known ? (running ? "运行" : "停止") : "未知", state_color, 742, 12, 110);
    }
}

static void draw_settings(void)
{
    char endpoint[96];
    snprintf(endpoint, sizeof(endpoint), "%s:%d", s_endpoint_host, s_endpoint_port);

    lv_obj_t *hero = card(s_content, 120, 56, 784, 230, PAGE_THEME[PAGE_SETTINGS].accent, lv_color_hex(0xF2FBFF));
    label(hero, "Web 后台", lv_color_hex(0x1B2826), 38, 40, 240);
    label(hero, s_web_url, PAGE_THEME[PAGE_SETTINGS].accent, 38, 96, 560);
    label(hero, "打开上方地址，配置 NAS 地址、Token 和刷新间隔。", lv_color_hex(0x65726F), 38, 154, 650);

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
    lv_label_set_text(s_online_text, s_online ? "在线" : "离线");
    lv_obj_set_style_bg_color(s_online_badge, s_online ? lv_color_hex(0x35A86D) : lv_color_hex(0xE85D62), 0);

    char power[32];
    format_power_status(power, sizeof(power));
    lv_label_set_text(s_power_text, power);
    lv_obj_set_style_bg_color(s_power_badge, power_color(), 0);

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

void ui_set_power_status(const power_status_t *status)
{
    if (status == NULL) {
        return;
    }
    if (!board_lvgl_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }
    s_power_status = *status;
    if (s_power_badge != NULL) {
        refresh_header();
    }
    board_lvgl_unlock();
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

    s_title_label = label_font(s_header, PAGE_THEME[s_page].name, FONT_BIG, lv_color_hex(0x1B2826), 28, 12, 120);
    s_hint_label = label(s_header, "", lv_color_hex(0x4E605C), 166, 20, 340);

    s_online_badge = status_badge(s_header, "离线", lv_color_hex(0xE85D62), 688, 13, 88, &s_online_text);
    s_power_badge = status_badge(s_header, "电量 -- 未知", lv_color_hex(0x4B95D9), 792, 13, 212, &s_power_text);

    s_page_label = label(s_header, "", lv_color_hex(0x1B2826), 640, 42, 42);
    lv_obj_set_style_text_align(s_page_label, LV_TEXT_ALIGN_RIGHT, 0);

    for (int i = 0; i < PAGE_COUNT; ++i) {
        s_page_dots[i] = lv_obj_create(s_header);
        lv_obj_remove_style_all(s_page_dots[i]);
        lv_obj_set_pos(s_page_dots[i], 526 + i * 14, 51);
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
    push_history();
    strlcpy(s_message, online ? "数据已刷新" : "NAS 暂时离线", sizeof(s_message));
    refresh_header();
    draw_page();
    board_lvgl_unlock();
}
