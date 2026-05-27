#include "ui.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "board_5b.h"
#include "esp_log.h"
#include "lvgl.h"
#include "sdkconfig.h"

typedef enum {
    PAGE_OVERVIEW = 0,
    PAGE_PERFORMANCE,
    PAGE_STORAGE,
    PAGE_DRIVES,
    PAGE_NVME,
    PAGE_NETWORK,
    PAGE_APPS,
    PAGE_SETTINGS,
    PAGE_COUNT,
} ui_page_t;

typedef struct {
    lv_color_t accent;
    const char *name;
} page_theme_t;

LV_FONT_DECLARE(lv_font_nas_cn_18);
#define FONT_CN (&lv_font_nas_cn_18)

#define SCREEN_W 1024
#define SCREEN_H 600
#define HEADER_H 64
#define CONTENT_Y HEADER_H
#define CONTENT_H (SCREEN_H - HEADER_H)
#define UI_LOCK_TIMEOUT_MS 1000

static const char __attribute__((unused)) *TAG = "ui";

static const lv_color_t BG_DARK        = LV_COLOR_MAKE(0x0F, 0x11, 0x17);
static const lv_color_t CARD_BG        = LV_COLOR_MAKE(0x18, 0x1B, 0x22);
static const lv_color_t CARD_BORDER    = LV_COLOR_MAKE(0x25, 0x29, 0x32);
static const lv_color_t HEADER_BG      = LV_COLOR_MAKE(0x13, 0x16, 0x1C);
static const lv_color_t TEXT_PRIMARY   = LV_COLOR_MAKE(0xE8, 0xEB, 0xEE);
static const lv_color_t TEXT_SECONDARY = LV_COLOR_MAKE(0x7D, 0x84, 0x91);
static const lv_color_t TEXT_MUTED     = LV_COLOR_MAKE(0x4E, 0x55, 0x62);
static const lv_color_t BAR_BG         = LV_COLOR_MAKE(0x1F, 0x23, 0x2B);
static const lv_color_t CHIP_BG        = LV_COLOR_MAKE(0x1F, 0x23, 0x2B);
static const lv_color_t WHITE          = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
static const lv_color_t GREEN_OK       = LV_COLOR_MAKE(0x2E, 0xCC, 0x71);
static const lv_color_t RED_CRIT       = LV_COLOR_MAKE(0xE7, 0x4C, 0x3C);
static const lv_color_t AMBER_WARN     = LV_COLOR_MAKE(0xF3, 0x9C, 0x12);
static const lv_color_t GRAY_UNKNOWN   = LV_COLOR_MAKE(0x7D, 0x84, 0x91);
static const lv_color_t BLUE_ACCENT    = LV_COLOR_MAKE(0x34, 0x98, 0xDB);
static const lv_color_t PURPLE_ACCENT  = LV_COLOR_MAKE(0x9B, 0x59, 0xB6);
static const lv_color_t TEAL_ACCENT    = LV_COLOR_MAKE(0x1A, 0xBC, 0x9C);
static const lv_color_t ORANGE_ACCENT  = LV_COLOR_MAKE(0xE6, 0x7E, 0x22);

static const page_theme_t PAGE_THEMES[PAGE_COUNT] = {
    [PAGE_OVERVIEW]     = {LV_COLOR_MAKE(0xE7, 0x4C, 0x3C), "\u603B\u89C8"},
    [PAGE_PERFORMANCE]  = {LV_COLOR_MAKE(0x34, 0x98, 0xDB), "\u6027\u80FD"},
    [PAGE_STORAGE]      = {LV_COLOR_MAKE(0xF3, 0x9C, 0x12), "\u5B58\u50A8"},
    [PAGE_DRIVES]       = {LV_COLOR_MAKE(0x2E, 0xCC, 0x71), "\u786C\u76D8"},
    [PAGE_NVME]         = {LV_COLOR_MAKE(0x9B, 0x59, 0xB6), "M.2"},
    [PAGE_NETWORK]      = {LV_COLOR_MAKE(0x1A, 0xBC, 0x9C), "\u7F51\u7EDC"},
    [PAGE_APPS]         = {LV_COLOR_MAKE(0xE6, 0x7E, 0x22), "\u5E94\u7528"},
    [PAGE_SETTINGS]     = {LV_COLOR_MAKE(0x7D, 0x84, 0x91), "\u8BBE\u7F6E"},
};

static lv_obj_t *s_header;
static lv_obj_t *s_content;
static lv_obj_t *s_page_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_endpoint_label;
static lv_obj_t *s_page_index_label;
static lv_obj_t *s_host_ta;
static lv_obj_t *s_port_ta;
static lv_obj_t *s_keyboard;
static lv_obj_t *s_settings_hint;

static ui_page_t s_page = PAGE_OVERVIEW;
static nas_status_t s_status;
static bool s_has_status;
static bool s_online;
static char s_message[96] = "\u6B63\u5728\u542F\u52A8";
static char s_endpoint_host[64] = CONFIG_NAS_DISPLAY_API_HOST;
static int s_endpoint_port = CONFIG_NAS_DISPLAY_API_PORT;
static ui_endpoint_save_cb_t s_save_cb;
static void *s_save_user_data;

static lv_style_t s_style_screen;
static lv_style_t s_style_card;
static lv_style_t s_style_chip;
static lv_style_t s_style_textarea;
static lv_style_t s_style_button;
static lv_style_t s_style_bar_bg;
static lv_style_t s_style_bar_ind;

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
        return "\u672A\u77E5";
    }
    if (text_equals(value, "ok") || text_equals(value, "good") || text_equals(value, "healthy") ||
        text_equals(value, "passed") || text_equals(value, "pass") || text_equals(value, "clean") ||
        text_equals(value, "active") || text_equals(value, "online") || text_equals(value, "up")) {
        return "\u6B63\u5E38";
    }
    if (text_equals(value, "warning") || text_equals(value, "warn") || text_equals(value, "degraded")) {
        return "\u8B66\u544A";
    }
    if (text_equals(value, "critical") || text_equals(value, "error") || text_equals(value, "failed") ||
        text_equals(value, "unhealthy") || text_equals(value, "down")) {
        return "\u5F02\u5E38";
    }
    if (text_equals(value, "running"))      return "\u8FD0\u884C\u4E2D";
    if (text_equals(value, "stopped") || text_equals(value, "exited")) return "\u5DF2\u505C\u6B62";
    if (text_equals(value, "resync") || text_equals(value, "syncing")) return "\u540C\u6B65\u4E2D";
    if (text_equals(value, "rebuild") || text_equals(value, "rebuilding") || text_equals(value, "repairing")) return "\u91CD\u5EFA\u4E2D";
    if (text_equals(value, "checking") || text_equals(value, "check")) return "\u68C0\u67E5\u4E2D";
    if (text_equals(value, "enabled"))      return "\u5DF2\u542F\u7528";
    if (text_equals(value, "disabled") || text_equals(value, "inactive")) return "\u672A\u542F\u7528";
    if (text_equals(value, "idle"))         return "\u7A7A\u95F2";
    if (text_equals(value, "missing"))      return "\u7F3A\u5931";
    if (text_equals(value, "disconnected"))  return "\u672A\u63A5\u5165";
    return "\u672A\u77E5";
}

static const char *drive_type_text(const char *type)
{
    if (text_equals(type, "hdd"))  return "\u673A\u68B0\u76D8";
    if (text_equals(type, "ssd"))  return "\u56FA\u6001\u76D8";
    if (text_equals(type, "nvme")) return "NVMe";
    return safe_text(type, "\u786C\u76D8");
}

static lv_color_t color_for_health(const char *health)
{
    if (health == NULL || health[0] == '\0') return GRAY_UNKNOWN;
    if (text_equals(health, "critical") || text_equals(health, "failed") ||
        text_equals(health, "unhealthy") || text_equals(health, "error") || text_equals(health, "down")) return RED_CRIT;
    if (text_equals(health, "warning") || text_equals(health, "warn") || text_equals(health, "degraded")) return AMBER_WARN;
    if (text_equals(health, "ok") || text_equals(health, "good") || text_equals(health, "healthy") ||
        text_equals(health, "passed") || text_equals(health, "clean") || text_equals(health, "online") ||
        text_equals(health, "up") || text_equals(health, "running")) return GREEN_OK;
    return GRAY_UNKNOWN;
}

static lv_color_t status_color(const char *status)
{
    if (status == NULL) return GRAY_UNKNOWN;
    if (text_equals(status, "up") || text_equals(status, "online")) return GREEN_OK;
    if (text_equals(status, "down") || text_equals(status, "disconnected")) return TEXT_MUTED;
    return GRAY_UNKNOWN;
}

static int pct_i(float value)
{
    if (value < 0.0f) return 0;
    if (value > 100.0f) return 100;
    return (int)(value + 0.5f);
}

static void format_pct(float value, char *out, size_t out_size)
{
    if (value < 0.0f) { snprintf(out, out_size, "--"); return; }
    snprintf(out, out_size, "%.0f%%", value);
}

static void format_temp(float value, char *out, size_t out_size)
{
    if (value <= 0.0f) { snprintf(out, out_size, "--"); return; }
    snprintf(out, out_size, "%.0f\u2103", value);
}

static void format_uptime_cn(uint32_t seconds, char *out, size_t out_size)
{
    uint32_t days = seconds / 86400;
    uint32_t hours = (seconds % 86400) / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    if (days > 0)
        snprintf(out, out_size, "%lu\u5929%lu\u5C0F\u65F6", (unsigned long)days, (unsigned long)hours);
    else if (hours > 0)
        snprintf(out, out_size, "%lu\u5C0F\u65F6%lu\u5206", (unsigned long)hours, (unsigned long)minutes);
    else
        snprintf(out, out_size, "%lu\u5206\u949F", (unsigned long)minutes);
}

/* ---- helper widgets ---- */

static lv_obj_t *label(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(obj, FONT_CN, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_letter_space(obj, 0, 0);
    lv_obj_set_pos(obj, x, y);
    if (w > 0) lv_obj_set_width(obj, w);
    return obj;
}

static lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t accent, const char *title)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_add_style(obj, &s_style_card, 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);

    if (title != NULL && title[0] != '\0') {
        label(obj, title, accent, 18, 16, w - 36);
    }
    return obj;
}

static void chip(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_add_style(c, &s_style_chip, 0);
    lv_obj_set_style_border_color(c, color, 0);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, 32);
    lv_obj_add_flag(c, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *t = label(c, text, color, 10, 6, w - 20);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
}

static void bar(lv_obj_t *parent, int x, int y, int w, int value, lv_color_t color)
{
    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 12);
    lv_bar_set_range(b, 0, 100);
    lv_bar_set_value(b, value, LV_ANIM_OFF);
    lv_obj_add_style(b, &s_style_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(b, &s_style_bar_ind, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(b, color, LV_PART_INDICATOR);
}

static void kpi(lv_obj_t *parent, const char *label_text, const char *value, int x, int y, int w)
{
    label(parent, label_text, TEXT_SECONDARY, x, y, w);
    label(parent, value, TEXT_PRIMARY, x, y + 22, w);
}

static void kpi_temp(lv_obj_t *parent, const char *label_text, float temp, int x, int y, int w)
{
    char buf[16];
    format_temp(temp, buf, sizeof(buf));
    kpi(parent, label_text, buf, x, y, w);
}

static void status_dot(lv_obj_t *parent, const char *status_str, int x, int y)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_style_bg_color(dot, status_color(status_str), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(dot, x, y);
    lv_obj_set_size(dot, 10, 10);
}

/* ---- page: no data ---- */

static void draw_no_data(const char *text)
{
    const page_theme_t *theme = &PAGE_THEMES[s_page];
    lv_obj_t *empty = card(s_content, 212, 160, 600, 180, theme->accent, "\u7B49\u5F85\u6570\u636E");
    label(empty, text, TEXT_PRIMARY, 24, 56, 552);
    label(empty, "\u8BF7\u786E\u8BA4 NAS \u7AEF Docker Agent \u5DF2\u542F\u52A8\uFF0C\u5E76\u4E14\u8BBE\u5907\u80FD\u8BBF\u95EE\u8BE5\u5730\u5740\u3002", TEXT_SECONDARY, 24, 88, 552);
}

static void clear_content(void)
{
    lv_obj_clean(s_content);
    s_host_ta = NULL;
    s_port_ta = NULL;
    s_settings_hint = NULL;
    if (s_keyboard != NULL && lv_obj_is_valid(s_keyboard)) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---- page: overview ---- */

static void draw_overview(void)
{
    char value[64];
    char buf[64];
    lv_obj_t *hero = card(s_content, 24, 24, 452, 224, color_for_health(s_status.nas.health), "NAS \u72B6\u6001");
    label(hero, safe_text(s_status.nas.hostname, "NAS \u4E3B\u673A"), TEXT_PRIMARY, 22, 52, 390);
    status_dot(hero, s_status.nas.health, 22, 73);
    snprintf(value, sizeof(value), "%s  \u00B7  %s", health_text(s_status.nas.health),
             safe_text(s_status.nas.primary_ip, "--"));
    label(hero, value, TEXT_SECONDARY, 38, 68, 380);
    format_uptime_cn(s_status.nas.uptime_sec, value, sizeof(value));
    kpi(hero, "\u8FD0\u884C\u65F6\u95F4", value, 22, 104, 200);
    snprintf(value, sizeof(value), "%d \u6761\u544A\u8B66", s_status.nas.alert_count);
    chip(hero, value, s_status.nas.alert_count > 0 ? RED_CRIT : GREEN_OK, 22, 166, 160);

    /* cpu + memory card */
    lv_obj_t *res = card(s_content, 500, 24, 252, 224, BLUE_ACCENT, "\u8D44\u6E90");
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    kpi(res, "CPU", value, 18, 52, 120);
    bar(res, 18, 100, 192, pct_i(s_status.cpu.usage_pct), BLUE_ACCENT);
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    kpi(res, "\u5185\u5B58", value, 18, 128, 120);
    bar(res, 18, 174, 192, pct_i(s_status.memory.used_pct), PURPLE_ACCENT);

    /* storage card — all pools aggregated */
    lv_obj_t *storage_card = card(s_content, 776, 24, 224, 224, AMBER_WARN, "\u5BB9\u91CF");
    if (s_status.storage.pool_count > 0) {
        uint64_t total_sum = 0, free_sum = 0;
        for (int i = 0; i < s_status.storage.pool_count; ++i) {
            total_sum += s_status.storage.pools[i].total_bytes;
            free_sum += s_status.storage.pools[i].free_bytes;
        }
        float used_pct = total_sum > 0 ? (float)(total_sum - free_sum) / (float)total_sum * 100.0f : 0.0f;
        char total_str[24], free_str[24];
        nas_format_bytes(total_sum, total_str, sizeof(total_str));
        nas_format_bytes(free_sum, free_str, sizeof(free_str));
        format_pct(used_pct, value, sizeof(value));
        kpi(storage_card, "\u5DF2\u7528", value, 18, 52, 160);
        bar(storage_card, 18, 100, 166, pct_i(used_pct), AMBER_WARN);
        snprintf(buf, sizeof(buf), "%s / %s", total_str, free_str);
        label(storage_card, buf, TEXT_PRIMARY, 18, 128, 170);
        label(storage_card, "\u603B\u5BB9\u91CF / \u5269\u4F59", TEXT_SECONDARY, 18, 156, 170);
    } else {
        label(storage_card, "\u6682\u65E0\u5B58\u50A8\u6C60\u6570\u636E", TEXT_SECONDARY, 18, 80, 170);
    }

    /* network card */
    lv_obj_t *net = card(s_content, 24, 272, 310, 210, TEAL_ACCENT, "\u7F51\u7EDC\u901F\u7387");
    nas_format_bps(s_status.network.total_rx_bps, buf, sizeof(buf));
    kpi(net, "\u4E0B\u8F7D", buf, 18, 52, 130);
    nas_format_bps(s_status.network.total_tx_bps, buf, sizeof(buf));
    kpi(net, "\u4E0A\u4F20", buf, 168, 52, 130);
    snprintf(value, sizeof(value), "%d \u4E2A\u7F51\u53E3", s_status.network.interface_count);
    chip(net, value, TEAL_ACCENT, 18, 136, 130);

    /* drives summary */
    lv_obj_t *drv = card(s_content, 358, 272, 310, 210, GREEN_OK, "\u78C1\u76D8");
    kpi_temp(drv, "CPU \u6E29\u5EA6", s_status.cpu.temperature_c, 18, 52, 140);
    if (s_status.drive_count > 0)
        kpi_temp(drv, "\u786C\u76D8\u6E29\u5EA6", s_status.drives[0].temperature_c, 168, 52, 140);
    snprintf(value, sizeof(value), "%d \u5757 HDD/SSD  \u00B7  %d \u5757 M.2", s_status.drive_count, s_status.nvme_count);
    chip(drv, value, GREEN_OK, 18, 136, 240);

    /* apps summary */
    lv_obj_t *apps = card(s_content, 692, 272, 308, 210, ORANGE_ACCENT, "\u5E94\u7528\u670D\u52A1");
    snprintf(value, sizeof(value), "%d \u8FD0\u884C / %d \u505C\u6B62",
             s_status.workloads.docker.running, s_status.workloads.docker.stopped);
    kpi(apps, "Docker", value, 18, 52, 220);
    snprintf(value, sizeof(value), "\u5907\u4EFD %d \u00B7 \u5FEB\u7167 %d",
             s_status.workloads.backup_count, s_status.workloads.snapshot_count);
    kpi(apps, "\u4EFB\u52A1", value, 18, 118, 220);
}

/* ---- page: performance ---- */

static void draw_performance(void)
{
    char value[64];

    lv_obj_t *cpu_card = card(s_content, 24, 24, 472, 234, BLUE_ACCENT, "CPU");
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    kpi(cpu_card, "\u5360\u7528\u7387", value, 24, 54, 140);
    bar(cpu_card, 24, 108, 400, pct_i(s_status.cpu.usage_pct), BLUE_ACCENT);
    format_temp(s_status.cpu.temperature_c, value, sizeof(value));
    kpi(cpu_card, "\u6E29\u5EA6", value, 234, 54, 100);
    snprintf(value, sizeof(value), "%d \u6838\u5FC3", s_status.cpu.core_count);
    kpi(cpu_card, "\u6838\u5FC3\u6570", value, 24, 146, 120);
    snprintf(value, sizeof(value), "%.2f / %.2f / %.2f", s_status.cpu.load_one, s_status.cpu.load_five, s_status.cpu.load_fifteen);
    kpi(cpu_card, "\u8D1F\u8F7D 1/5/15 min", value, 180, 146, 270);

    lv_obj_t *mem_card = card(s_content, 526, 24, 474, 234, PURPLE_ACCENT, "\u5185\u5B58");
    char total[24], used_b[24], cache[24];
    nas_format_bytes(s_status.memory.total_bytes, total, sizeof(total));
    nas_format_bytes(s_status.memory.used_bytes, used_b, sizeof(used_b));
    nas_format_bytes(s_status.memory.cache_bytes, cache, sizeof(cache));
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    kpi(mem_card, "\u5185\u5B58\u5360\u7528", value, 24, 54, 140);
    bar(mem_card, 24, 108, 400, pct_i(s_status.memory.used_pct), PURPLE_ACCENT);
    snprintf(value, sizeof(value), "%s / %s", used_b, total);
    kpi(mem_card, "\u5DF2\u7528 / \u603B\u91CF", value, 24, 146, 250);
    kpi(mem_card, "\u7F13\u5B58", cache, 310, 146, 150);

    lv_obj_t *swap_card = card(s_content, 24, 286, 472, 196, AMBER_WARN, "Swap");
    char swap_total[24], swap_used[24];
    nas_format_bytes(s_status.memory.swap_total_bytes, swap_total, sizeof(swap_total));
    nas_format_bytes(s_status.memory.swap_used_bytes, swap_used, sizeof(swap_used));
    format_pct(s_status.memory.swap_used_pct, value, sizeof(value));
    kpi(swap_card, "Swap \u5360\u7528", value, 24, 54, 160);
    bar(swap_card, 24, 108, 400, pct_i(s_status.memory.swap_used_pct), AMBER_WARN);
    snprintf(value, sizeof(value), "%s / %s", swap_used, swap_total);
    kpi(swap_card, "\u5DF2\u7528 / \u603B\u91CF", value, 226, 54, 210);

    lv_obj_t *health_card = card(s_content, 526, 286, 474, 196, color_for_health(s_status.cpu.health), "\u5065\u5EB7\u72B6\u6001");
    chip(health_card, health_text(s_status.cpu.health), color_for_health(s_status.cpu.health), 24, 60, 140);
    chip(health_card, health_text(s_status.memory.health), color_for_health(s_status.memory.health), 194, 60, 140);
    label(health_card, "CPU / \u5185\u5B58\u72B6\u6001\u7531 NAS Agent \u6C47\u603B\u5224\u65AD\u3002", TEXT_SECONDARY, 24, 118, 400);
}

/* ---- page: storage ---- */

static void draw_storage(void)
{
    char value[96];

    lv_obj_t *pool_card = card(s_content, 24, 24, 976, 220, AMBER_WARN, "\u5B58\u50A8\u6C60 / RAID");
    if (s_status.storage.pool_count == 0) {
        label(pool_card, "\u6682\u65E0\u5B58\u50A8\u6C60\u6570\u636E", TEXT_SECONDARY, 24, 72, 400);
    }
    for (int i = 0; i < s_status.storage.pool_count && i < 3; ++i) {
        const nas_pool_t *pool = &s_status.storage.pools[i];
        int x = 24 + i * 310;
        char total_str[24], free_str[24];
        nas_format_bytes(pool->total_bytes, total_str, sizeof(total_str));
        nas_format_bytes(pool->free_bytes, free_str, sizeof(free_str));
        label(pool_card, safe_text(pool->name, pool->id), TEXT_PRIMARY, x, 52, 270);
        snprintf(value, sizeof(value), "%s \u00B7 %s", safe_text(pool->raid_type, "--"), health_text(pool->raid_status));
        label(pool_card, value, TEXT_SECONDARY, x, 84, 270);
        format_pct(pool->used_pct, value, sizeof(value));
        kpi(pool_card, "\u5DF2\u7528", value, x, 116, 90);
        bar(pool_card, x + 86, 128, 174, pct_i(pool->used_pct), AMBER_WARN);
        snprintf(value, sizeof(value), "%s \u5269\u4F59 %s", total_str, free_str);
        label(pool_card, value, TEXT_PRIMARY, x, 156, 270);
    }

    lv_obj_t *vol_card = card(s_content, 24, 274, 976, 208, BLUE_ACCENT, "\u5377\u5BB9\u91CF");
    if (s_status.storage.volume_count == 0) {
        label(vol_card, "\u6682\u65E0\u5377\u6570\u636E", TEXT_SECONDARY, 24, 72, 400);
    }
    for (int i = 0; i < s_status.storage.volume_count && i < 4; ++i) {
        const nas_volume_t *vol = &s_status.storage.volumes[i];
        int cx = 24 + (i % 2) * 470;
        int cy = 52 + (i / 2) * 76;
        char used_str[24], free_str[24];
        nas_format_bytes(vol->used_bytes, used_str, sizeof(used_str));
        nas_format_bytes(vol->free_bytes, free_str, sizeof(free_str));
        snprintf(value, sizeof(value), "%s \u00B7 %s", safe_text(vol->name, vol->id), safe_text(vol->filesystem, "--"));
        label(vol_card, value, TEXT_PRIMARY, cx, cy, 280);
        format_pct(vol->used_pct, value, sizeof(value));
        label(vol_card, value, TEXT_PRIMARY, cx + 300, cy, 60);
        bar(vol_card, cx, cy + 28, 360, pct_i(vol->used_pct), BLUE_ACCENT);
        snprintf(value, sizeof(value), "\u5DF2\u7528 %s / \u5269\u4F59 %s", used_str, free_str);
        label(vol_card, value, TEXT_SECONDARY, cx, cy + 48, 360);
    }
}

/* ---- page: drives ---- */

static void draw_drives(void)
{
    if (s_status.drive_count == 0) {
        draw_no_data("\u6682\u65E0 HDD / SSD \u6570\u636E");
        return;
    }

    char value[96];
    for (int i = 0; i < s_status.drive_count && i < 8; ++i) {
        const nas_drive_t *drive = &s_status.drives[i];
        int col = i % 4, row = i / 4;
        int x = 24 + col * 248, y = 24 + row * 224;
        lv_obj_t *c = card(s_content, x, y, 224, 196, color_for_health(drive->health), safe_text(drive->bay, drive->id));
        snprintf(value, sizeof(value), "%s \u00B7 %s", drive_type_text(drive->type), health_text(drive->smart_status));
        label(c, value, TEXT_SECONDARY, 16, 48, 180);
        kpi_temp(c, "\u6E29\u5EA6", drive->temperature_c, 16, 76, 80);
        char cap[24];
        nas_format_bytes(drive->capacity_bytes, cap, sizeof(cap));
        kpi(c, "\u5BB9\u91CF", cap, 116, 76, 86);
        snprintf(value, sizeof(value), "%d", drive->bad_sector_count);
        kpi(c, "\u574F\u5757", value, 16, 132, 80);
        snprintf(value, sizeof(value), "%d \u5C0F\u65F6", drive->power_on_hours);
        kpi(c, "\u901A\u7535", value, 116, 132, 86);
    }
}

/* ---- page: nvme / M.2 ---- */

static void draw_nvme(void)
{
    if (s_status.nvme_count == 0) {
        draw_no_data("\u6682\u65E0 M.2 / NVMe \u6570\u636E");
        return;
    }

    char value[96];
    for (int i = 0; i < s_status.nvme_count && i < 6; ++i) {
        const nas_nvme_t *nvme = &s_status.nvme[i];
        int col = i % 3, row = i / 3;
        int x = 24 + col * 330, y = 24 + row * 224;
        lv_obj_t *c = card(s_content, x, y, 306, 196, color_for_health(nvme->health), safe_text(nvme->slot, nvme->id));
        label(c, safe_text(nvme->model, "NVMe \u56FA\u6001"), TEXT_PRIMARY, 16, 48, 250);
        char cap[24], used_str[24];
        nas_format_bytes(nvme->capacity_bytes, cap, sizeof(cap));
        nas_format_bytes(nvme->used_bytes, used_str, sizeof(used_str));
        kpi(c, "\u5BB9\u91CF", cap, 16, 82, 100);
        kpi(c, "\u5DF2\u7528", used_str, 156, 82, 100);
        kpi_temp(c, "\u6E29\u5EA6", nvme->temperature_c, 16, 138, 80);
        int wear = nvme->percentage_used_pct > 0 ? nvme->percentage_used_pct : nvme->wear_pct;
        snprintf(value, sizeof(value), "%d%% \u78E8\u635F", wear);
        kpi(c, "\u5BFF\u547D", value, 116, 138, 80);
        chip(c, health_text(nvme->cache_state), PURPLE_ACCENT, 206, 134, 76);
    }
}

/* ---- page: network ---- */

static void draw_network(void)
{
    char value[96];
    char rx[24], tx[24];

    lv_obj_t *summary = card(s_content, 24, 24, 976, 130, TEAL_ACCENT, "\u7F51\u7EDC\u603B\u89C8");
    nas_format_bps(s_status.network.total_rx_bps, rx, sizeof(rx));
    nas_format_bps(s_status.network.total_tx_bps, tx, sizeof(tx));
    kpi(summary, "\u4E0B\u8F7D\u901F\u5EA6", rx, 28, 54, 180);
    kpi(summary, "\u4E0A\u4F20\u901F\u5EA6", tx, 234, 54, 180);
    snprintf(value, sizeof(value), "%d \u4E2A\u7F51\u53E3", s_status.network.interface_count);
    kpi(summary, "\u7F51\u53E3\u6570\u91CF", value, 440, 54, 150);
    chip(summary, health_text(s_status.network.health), color_for_health(s_status.network.health), 616, 56, 110);

    for (int i = 0; i < s_status.network.interface_count && i < 6; ++i) {
        const nas_interface_t *iface = &s_status.network.interfaces[i];
        int col = i % 3, row = i / 3;
        int x = 24 + col * 330, y = 182 + row * 144;
        lv_color_t st_color = status_color(iface->status);
        lv_obj_t *c = card(s_content, x, y, 306, 126, st_color, safe_text(iface->name, "\u7F51\u53E3"));
        status_dot(c, iface->status, 16, 46);
        snprintf(value, sizeof(value), "%s  \u00B7  %d Mbps", health_text(iface->status), iface->link_speed_mbps);
        label(c, value, TEXT_SECONDARY, 30, 44, 250);
        label(c, safe_text(iface->ip, "--"), TEXT_PRIMARY, 16, 72, 250);
        snprintf(value, sizeof(value), "\u9519\u8BEF %d/%d  \u00B7  \u4E22\u5305 %d/%d",
                 iface->rx_errors, iface->tx_errors, iface->rx_dropped, iface->tx_dropped);
        label(c, value, TEXT_SECONDARY, 16, 98, 270);
    }
}

/* ---- page: apps ---- */

static void draw_apps(void)
{
    char value[96];

    lv_obj_t *summary = card(s_content, 24, 24, 976, 138, ORANGE_ACCENT, "Docker / \u4EFB\u52A1");
    snprintf(value, sizeof(value), "%d \u4E2A\u8FD0\u884C", s_status.workloads.docker.running);
    kpi(summary, "\u8FD0\u884C\u5BB9\u5668", value, 28, 56, 160);
    snprintf(value, sizeof(value), "%d \u4E2A\u505C\u6B62", s_status.workloads.docker.stopped);
    kpi(summary, "\u505C\u6B62\u5BB9\u5668", value, 230, 56, 160);
    snprintf(value, sizeof(value), "%d \u4E2A\u5F02\u5E38", s_status.workloads.docker.unhealthy);
    kpi(summary, "\u5F02\u5E38\u5BB9\u5668", value, 432, 56, 160);
    snprintf(value, sizeof(value), "\u5907\u4EFD %d \u00B7 \u5FEB\u7167 %d", s_status.workloads.backup_count, s_status.workloads.snapshot_count);
    kpi(summary, "\u5907\u4EFD / \u5FEB\u7167", value, 634, 56, 240);

    lv_obj_t *list = card(s_content, 24, 194, 976, 288, BLUE_ACCENT, "\u670D\u52A1\u5217\u8868");
    if (s_status.workloads.docker.container_count == 0) {
        label(list, "\u6682\u65E0\u5BB9\u5668\u6570\u636E", TEXT_SECONDARY, 24, 74, 300);
    }
    for (int i = 0; i < s_status.workloads.docker.container_count && i < NAS_MAX_CONTAINERS && i < 10; ++i) {
        const nas_container_t *container = &s_status.workloads.docker.containers[i];
        int col = i % 2, row = i / 2;
        int x = 24 + col * 460, y = 54 + row * 44;
        label(list, safe_text(container->name, "\u5BB9\u5668"), TEXT_PRIMARY, x, y, 260);
        snprintf(value, sizeof(value), "%s / %s", health_text(container->state), health_text(container->health));
        chip(list, value, color_for_health(container->health), x + 280, y - 2, 130);
    }
}

/* ---- page: settings ---- */

static bool valid_endpoint_host(const char *host)
{
    if (host == NULL || host[0] == '\0' || strlen(host) >= sizeof(s_endpoint_host)) return false;
    for (const char *p = host; *p != '\0'; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (isspace(ch) || ch == '/' || ch == ':' || ch == '?' || ch == '#') return false;
    }
    return true;
}

static int parse_port(const char *text)
{
    if (text == NULL || text[0] == '\0') return -1;
    char *end = NULL;
    long port = strtol(text, &end, 10);
    if (end == text || *end != '\0' || port <= 0 || port > 65535) return -1;
    return (int)port;
}

static void set_settings_hint(const char *text, lv_color_t color)
{
    if (s_settings_hint == NULL) return;
    lv_label_set_text(s_settings_hint, text);
    lv_obj_set_style_text_color(s_settings_hint, color, 0);
}

static void keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_add_flag(lv_event_get_target(event), LV_OBJ_FLAG_HIDDEN);
}

static void show_keyboard_for(lv_obj_t *textarea)
{
    if (s_keyboard == NULL || textarea == NULL) return;
    lv_keyboard_set_textarea(s_keyboard, textarea);
    if (textarea == s_port_ta)
        lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_NUMBER);
    else
        lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
}

static void textarea_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED)
        show_keyboard_for(lv_event_get_target(event));
}

static void input_hotspot_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) return;
    lv_obj_t *textarea = (lv_obj_t *)lv_event_get_user_data(event);
    show_keyboard_for(textarea);
}

static void add_input_hotspot(lv_obj_t *parent, lv_obj_t *textarea, int x, int y, int w, int h)
{
    lv_obj_t *hotspot = lv_obj_create(parent);
    lv_obj_remove_style_all(hotspot);
    lv_obj_set_pos(hotspot, x, y);
    lv_obj_set_size(hotspot, w, h);
    lv_obj_set_ext_click_area(hotspot, 8);
    lv_obj_add_flag(hotspot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hotspot, input_hotspot_event_cb, LV_EVENT_ALL, textarea);
}

static void ensure_keyboard(void)
{
    if (s_keyboard != NULL && lv_obj_is_valid(s_keyboard)) return;
    s_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(s_keyboard, 900, 220);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_text_font(s_keyboard, &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_color(s_keyboard, CARD_BG, 0);
    lv_obj_set_style_bg_opa(s_keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_keyboard, 2, 0);
    lv_obj_set_style_border_color(s_keyboard, BLUE_ACCENT, 0);
    lv_obj_add_event_cb(s_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void save_settings_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || s_host_ta == NULL || s_port_ta == NULL) return;

    const char *host = lv_textarea_get_text(s_host_ta);
    int port = parse_port(lv_textarea_get_text(s_port_ta));
    if (!valid_endpoint_host(host) || port < 0) {
        set_settings_hint("\u5730\u5740\u6216\u7AEF\u53E3\u683C\u5F0F\u4E0D\u6B63\u786E", RED_CRIT);
        return;
    }
    bool saved = s_save_cb != NULL ? s_save_cb(host, port, s_save_user_data) : false;
    if (!saved) {
        set_settings_hint("\u4FDD\u5B58\u5931\u8D25\uFF0C\u8BF7\u7A0D\u540E\u91CD\u8BD5", RED_CRIT);
        return;
    }
    strlcpy(s_endpoint_host, host, sizeof(s_endpoint_host));
    s_endpoint_port = port;
    strlcpy(s_message, "\u63A5\u53E3\u5730\u5740\u5DF2\u4FDD\u5B58", sizeof(s_message));
    set_settings_hint("\u5DF2\u4FDD\u5B58\uFF0C\u4E0B\u4E00\u6B21\u5237\u65B0\u4F1A\u4F7F\u7528\u65B0\u5730\u5740", GREEN_OK);
    if (s_keyboard != NULL) lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(s_host_ta, LV_STATE_FOCUSED);
    lv_obj_clear_state(s_port_ta, LV_STATE_FOCUSED);
    lv_obj_invalidate(s_header);
}

static void draw_settings(void)
{
    char value[24];
    ensure_keyboard();

    lv_obj_t *panel = card(s_content, 104, 34, 816, 280, GRAY_UNKNOWN, "NAS Agent \u5730\u5740");
    label(panel, "\u4E3B\u673A\u5730\u5740", TEXT_SECONDARY, 30, 58, 120);
    s_host_ta = lv_textarea_create(panel);
    lv_obj_add_style(s_host_ta, &s_style_textarea, 0);
    lv_obj_set_pos(s_host_ta, 148, 44);
    lv_obj_set_size(s_host_ta, 430, 52);
    lv_textarea_set_one_line(s_host_ta, true);
    lv_textarea_set_max_length(s_host_ta, sizeof(s_endpoint_host) - 1);
    lv_textarea_set_accepted_chars(s_host_ta, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_");
    lv_textarea_set_placeholder_text(s_host_ta, "IP \u6216\u57DF\u540D");
    lv_textarea_set_text(s_host_ta, s_endpoint_host);
    lv_obj_add_flag(s_host_ta, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_host_ta, textarea_event_cb, LV_EVENT_ALL, NULL);

    label(panel, "\u7AEF\u53E3", TEXT_SECONDARY, 30, 128, 120);
    s_port_ta = lv_textarea_create(panel);
    lv_obj_add_style(s_port_ta, &s_style_textarea, 0);
    lv_obj_set_pos(s_port_ta, 148, 114);
    lv_obj_set_size(s_port_ta, 160, 52);
    lv_textarea_set_one_line(s_port_ta, true);
    lv_textarea_set_max_length(s_port_ta, 5);
    lv_textarea_set_accepted_chars(s_port_ta, "0123456789");
    snprintf(value, sizeof(value), "%d", s_endpoint_port);
    lv_textarea_set_text(s_port_ta, value);
    lv_obj_add_flag(s_port_ta, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_port_ta, textarea_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *save = lv_btn_create(panel);
    lv_obj_add_style(save, &s_style_button, 0);
    lv_obj_set_style_bg_color(save, GREEN_OK, 0);
    lv_obj_set_pos(save, 616, 72);
    lv_obj_set_size(save, 150, 70);
    lv_obj_add_event_cb(save, save_settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_text = label(save, "\u4FDD\u5B58", WHITE, 0, 23, 150);
    lv_obj_set_style_text_align(save_text, LV_TEXT_ALIGN_CENTER, 0);

    s_settings_hint = label(panel, "\u652F\u6301\u57DF\u540D\u6216 IPv4 \u5730\u5740\uFF0C\u7AEF\u53E3\u8303\u56F4 1-65535", TEXT_MUTED, 30, 210, 720);

    lv_obj_t *current = card(s_content, 104, 344, 816, 74, GREEN_OK, "\u5F53\u524D\u8FDE\u63A5");
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "%s:%d", s_endpoint_host, s_endpoint_port);
    label(current, endpoint, TEXT_PRIMARY, 24, 38, 520);
    chip(current, s_online ? "\u5728\u7EBF" : "\u79BB\u7EBF", s_online ? GREEN_OK : RED_CRIT, 638, 28, 110);

    add_input_hotspot(panel, s_host_ta, 148, 44, 430, 52);
    add_input_hotspot(panel, s_port_ta, 148, 114, 160, 52);

    lv_keyboard_set_textarea(s_keyboard, s_host_ta);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
}

/* ---- navigation / lifecycle ---- */

static void draw_page(void)
{
    clear_content();
    if (!s_has_status && s_page != PAGE_SETTINGS) {
        const page_theme_t *theme = &PAGE_THEMES[s_page];
        lv_obj_t *empty = card(s_content, 212, 160, 600, 180, theme->accent, "\u7B49\u5F85\u6570\u636E");
        label(empty, "\u8FD8\u6CA1\u6709\u6536\u5230 NAS \u72B6\u6001\u6570\u636E", TEXT_PRIMARY, 24, 56, 552);
        label(empty, "\u8BF7\u786E\u8BA4 NAS \u7AEF Docker Agent \u5DF2\u542F\u52A8\uFF0C\u5E76\u4E14\u8BBE\u5907\u80FD\u8BBF\u95EE\u8BE5\u5730\u5740\u3002", TEXT_SECONDARY, 24, 88, 552);
        return;
    }
    switch (s_page) {
    case PAGE_OVERVIEW:    draw_overview();    break;
    case PAGE_PERFORMANCE: draw_performance(); break;
    case PAGE_STORAGE:     draw_storage();     break;
    case PAGE_DRIVES:      draw_drives();      break;
    case PAGE_NVME:        draw_nvme();        break;
    case PAGE_NETWORK:     draw_network();     break;
    case PAGE_APPS:        draw_apps();        break;
    case PAGE_SETTINGS:    draw_settings();    break;
    default: break;
    }
}

static void refresh_header(void)
{
    const page_theme_t *theme = &PAGE_THEMES[s_page];
    lv_label_set_text(s_page_label, theme->name);

    char endpoint[200];
    snprintf(endpoint, sizeof(endpoint), "%s:%d  %s", s_endpoint_host, s_endpoint_port, s_message);
    lv_label_set_text(s_endpoint_label, endpoint);

    lv_label_set_text(s_status_label, s_online ? "\u5728\u7EBF" : "\u79BB\u7EBF");
    lv_obj_set_style_bg_color(s_status_label, s_online ? GREEN_OK : RED_CRIT, 0);

    char page_index[32];
    snprintf(page_index, sizeof(page_index), "%d/%d", (int)s_page + 1, PAGE_COUNT);
    lv_label_set_text(s_page_index_label, page_index);
}

static void set_page(ui_page_t page)
{
    if (page >= PAGE_COUNT || page == s_page) return;
    s_page = page;
    refresh_header();
    draw_page();
}

static void gesture_event_cb(lv_event_t *event)
{
    (void)event;
    if (s_keyboard != NULL && !lv_obj_has_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN)) return;

    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_LEFT)
        set_page((ui_page_t)((s_page + 1) % PAGE_COUNT));
    else if (dir == LV_DIR_RIGHT)
        set_page((ui_page_t)((s_page + PAGE_COUNT - 1) % PAGE_COUNT));
}

/* ---- initialisation ---- */

static void init_styles(void)
{
    lv_style_init(&s_style_screen);
    lv_style_set_bg_color(&s_style_screen, BG_DARK);
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);

    lv_style_init(&s_style_card);
    lv_style_set_bg_color(&s_style_card, CARD_BG);
    lv_style_set_bg_opa(&s_style_card, LV_OPA_COVER);
    lv_style_set_radius(&s_style_card, 10);
    lv_style_set_border_width(&s_style_card, 1);
    lv_style_set_border_color(&s_style_card, CARD_BORDER);
    lv_style_set_pad_all(&s_style_card, 0);

    lv_style_init(&s_style_chip);
    lv_style_set_bg_color(&s_style_chip, CHIP_BG);
    lv_style_set_bg_opa(&s_style_chip, LV_OPA_COVER);
    lv_style_set_radius(&s_style_chip, 6);
    lv_style_set_border_width(&s_style_chip, 1);
    lv_style_set_pad_all(&s_style_chip, 0);

    lv_style_init(&s_style_textarea);
    lv_style_set_bg_color(&s_style_textarea, BG_DARK);
    lv_style_set_bg_opa(&s_style_textarea, LV_OPA_COVER);
    lv_style_set_radius(&s_style_textarea, 8);
    lv_style_set_border_width(&s_style_textarea, 2);
    lv_style_set_border_color(&s_style_textarea, CARD_BORDER);
    lv_style_set_pad_left(&s_style_textarea, 12);
    lv_style_set_pad_top(&s_style_textarea, 10);
    lv_style_set_text_font(&s_style_textarea, FONT_CN);
    lv_style_set_text_color(&s_style_textarea, TEXT_PRIMARY);

    lv_style_init(&s_style_button);
    lv_style_set_radius(&s_style_button, 8);
    lv_style_set_border_width(&s_style_button, 0);
    lv_style_set_shadow_width(&s_style_button, 0);

    lv_style_init(&s_style_bar_bg);
    lv_style_set_bg_color(&s_style_bar_bg, BAR_BG);
    lv_style_set_bg_opa(&s_style_bar_bg, LV_OPA_COVER);
    lv_style_set_radius(&s_style_bar_bg, 6);

    lv_style_init(&s_style_bar_ind);
    lv_style_set_radius(&s_style_bar_ind, 6);
}

void ui_set_endpoint_config(const char *host, int port)
{
    if (host != NULL && host[0] != '\0') strlcpy(s_endpoint_host, host, sizeof(s_endpoint_host));
    if (port > 0 && port <= 65535) s_endpoint_port = port;
}

void ui_set_endpoint_save_callback(ui_endpoint_save_cb_t callback, void *user_data)
{
    s_save_cb = callback;
    s_save_user_data = user_data;
}

void ui_init(void)
{
    if (!board_lvgl_lock(UI_LOCK_TIMEOUT_MS)) return;

    init_styles();
    nas_status_init(&s_status);

    lv_obj_t *screen = lv_scr_act();
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, &s_style_screen, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, gesture_event_cb, LV_EVENT_GESTURE, NULL);

    s_header = lv_obj_create(screen);
    lv_obj_set_style_bg_color(s_header, HEADER_BG, 0);
    lv_obj_set_style_bg_opa(s_header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_header, 0, 0);
    lv_obj_set_style_radius(s_header, 0, 0);
    lv_obj_set_style_pad_all(s_header, 0, 0);
    lv_obj_set_pos(s_header, 0, 0);
    lv_obj_set_size(s_header, SCREEN_W, HEADER_H);
    lv_obj_add_flag(s_header, LV_OBJ_FLAG_GESTURE_BUBBLE);

    s_page_label = label(s_header, PAGE_THEMES[s_page].name, PAGE_THEMES[s_page].accent, 24, 16, 100);
    s_endpoint_label = label(s_header, "", TEXT_SECONDARY, 140, 14, 580);
    s_status_label = label(s_header, "\u79BB\u7EBF", WHITE, 0, 0, 80);
    lv_obj_add_style(s_status_label, &s_style_chip, 0);
    lv_obj_set_style_bg_color(s_status_label, RED_CRIT, 0);
    lv_obj_set_style_border_width(s_status_label, 0, 0);
    lv_obj_set_pos(s_status_label, 814, 16);
    lv_obj_set_size(s_status_label, 80, 34);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);

    s_page_index_label = label(s_header, "", TEXT_MUTED, 914, 22, 90);

    s_content = lv_obj_create(screen);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_pos(s_content, 0, CONTENT_Y);
    lv_obj_set_size(s_content, SCREEN_W, CONTENT_H);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_content, LV_OBJ_FLAG_GESTURE_BUBBLE);

    refresh_header();
    draw_page();

    board_lvgl_unlock();
}

void ui_set_message(const char *message)
{
    if (!board_lvgl_lock(UI_LOCK_TIMEOUT_MS)) return;
    strlcpy(s_message, safe_text(message, ""), sizeof(s_message));
    refresh_header();
    if ((!s_has_status || s_page == PAGE_SETTINGS) &&
        (s_page != PAGE_SETTINGS || s_keyboard == NULL || lv_obj_has_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN))) {
        draw_page();
    }
    board_lvgl_unlock();
}

void ui_update_status(const nas_status_t *status, bool online)
{
    if (status == NULL) return;
    if (!board_lvgl_lock(UI_LOCK_TIMEOUT_MS)) return;
    s_status = *status;
    s_has_status = true;
    s_online = online;
    strlcpy(s_message, online ? "\u6570\u636E\u5DF2\u5237\u65B0" : "\u8FDE\u63A5\u5931\u8D25", sizeof(s_message));
    refresh_header();
    if (s_page != PAGE_SETTINGS || s_keyboard == NULL || lv_obj_has_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN)) {
        draw_page();
    }
    board_lvgl_unlock();
}
