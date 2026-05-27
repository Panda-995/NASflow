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
    PAGE_ENV,
    PAGE_APPS,
    PAGE_SETTINGS,
    PAGE_COUNT,
} ui_page_t;

typedef struct {
    lv_color_t accent;
    lv_color_t paper;
    const char *name;
} page_theme_t;

LV_FONT_DECLARE(lv_font_nas_cn_18);
#define FONT_CN (&lv_font_nas_cn_18)

#define SCREEN_W 1024
#define SCREEN_H 600
#define HEADER_H 70
#define CONTENT_Y HEADER_H
#define CONTENT_H (SCREEN_H - HEADER_H)
#define UI_LOCK_TIMEOUT_MS 1000

static const char *TAG = "ui";

static const page_theme_t PAGE_THEMES[PAGE_COUNT] = {
    [PAGE_OVERVIEW] = {LV_COLOR_MAKE(0xEC, 0x6F, 0x66), LV_COLOR_MAKE(0xFF, 0xF1, 0xDC), "总览"},
    [PAGE_PERFORMANCE] = {LV_COLOR_MAKE(0x24, 0x9F, 0x9C), LV_COLOR_MAKE(0xE7, 0xF8, 0xEF), "性能"},
    [PAGE_STORAGE] = {LV_COLOR_MAKE(0xF2, 0xB8, 0x4B), LV_COLOR_MAKE(0xFF, 0xF5, 0xD6), "存储"},
    [PAGE_DRIVES] = {LV_COLOR_MAKE(0x6C, 0x8A, 0xE4), LV_COLOR_MAKE(0xEC, 0xF2, 0xFF), "硬盘"},
    [PAGE_NVME] = {LV_COLOR_MAKE(0xDD, 0x73, 0xA8), LV_COLOR_MAKE(0xFF, 0xEE, 0xF6), "M.2"},
    [PAGE_NETWORK] = {LV_COLOR_MAKE(0x32, 0xA8, 0x67), LV_COLOR_MAKE(0xE7, 0xF8, 0xE8), "网络"},
    [PAGE_ENV] = {LV_COLOR_MAKE(0xE0, 0x8A, 0x3D), LV_COLOR_MAKE(0xFF, 0xF0, 0xE2), "环境"},
    [PAGE_APPS] = {LV_COLOR_MAKE(0x87, 0x66, 0xC9), LV_COLOR_MAKE(0xF2, 0xEC, 0xFF), "应用"},
    [PAGE_SETTINGS] = {LV_COLOR_MAKE(0x4C, 0x96, 0xD7), LV_COLOR_MAKE(0xEA, 0xF6, 0xFF), "设置"},
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
static char s_message[96] = "正在启动";
static char s_endpoint_host[64] = CONFIG_NAS_DISPLAY_API_HOST;
static int s_endpoint_port = CONFIG_NAS_DISPLAY_API_PORT;
static ui_endpoint_save_cb_t s_save_cb;
static void *s_save_user_data;

static lv_style_t s_style_screen;
static lv_style_t s_style_header;
static lv_style_t s_style_card;
static lv_style_t s_style_chip;
static lv_style_t s_style_textarea;
static lv_style_t s_style_button;

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
        text_equals(value, "passed") || text_equals(value, "pass") || text_equals(value, "clean") ||
        text_equals(value, "active") || text_equals(value, "online") || text_equals(value, "up")) {
        return "正常";
    }
    if (text_equals(value, "warning") || text_equals(value, "warn") || text_equals(value, "degraded")) {
        return "警告";
    }
    if (text_equals(value, "critical") || text_equals(value, "error") || text_equals(value, "failed") ||
        text_equals(value, "unhealthy") || text_equals(value, "down")) {
        return "异常";
    }
    if (text_equals(value, "running")) {
        return "运行中";
    }
    if (text_equals(value, "stopped") || text_equals(value, "exited")) {
        return "已停止";
    }
    if (text_equals(value, "resync") || text_equals(value, "syncing")) {
        return "同步中";
    }
    if (text_equals(value, "rebuild") || text_equals(value, "rebuilding") || text_equals(value, "repairing")) {
        return "重建中";
    }
    if (text_equals(value, "checking") || text_equals(value, "check")) {
        return "检查中";
    }
    if (text_equals(value, "enabled")) {
        return "已启用";
    }
    if (text_equals(value, "disabled") || text_equals(value, "inactive")) {
        return "未启用";
    }
    if (text_equals(value, "idle")) {
        return "空闲";
    }
    if (text_equals(value, "missing")) {
        return "缺失";
    }
    return "未知";
}

static const char *drive_type_text(const char *type)
{
    if (text_equals(type, "hdd")) {
        return "机械盘";
    }
    if (text_equals(type, "ssd")) {
        return "固态盘";
    }
    if (text_equals(type, "nvme")) {
        return "NVMe";
    }
    return safe_text(type, "硬盘");
}

static lv_color_t color_for_health(const char *health)
{
    if (health == NULL || health[0] == '\0') {
        return lv_color_hex(0x9AA6A3);
    }
    if (text_equals(health, "critical") || text_equals(health, "failed") || text_equals(health, "unhealthy") ||
        text_equals(health, "error") || text_equals(health, "down")) {
        return lv_color_hex(0xE65D62);
    }
    if (text_equals(health, "warning") || text_equals(health, "warn") || text_equals(health, "degraded")) {
        return lv_color_hex(0xF0AD3D);
    }
    if (text_equals(health, "ok") || text_equals(health, "good") || text_equals(health, "healthy") ||
        text_equals(health, "passed") || text_equals(health, "clean") || text_equals(health, "online") ||
        text_equals(health, "up") || text_equals(health, "running")) {
        return lv_color_hex(0x38A76B);
    }
    return lv_color_hex(0x9AA6A3);
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
        return;
    }
    snprintf(out, out_size, "%.0f%%", value);
}

static void format_temp(float value, char *out, size_t out_size)
{
    if (value <= 0.0f) {
        snprintf(out, out_size, "--");
        return;
    }
    snprintf(out, out_size, "%.0f℃", value);
}

static void format_uptime_cn(uint32_t seconds, char *out, size_t out_size)
{
    uint32_t days = seconds / 86400;
    uint32_t hours = (seconds % 86400) / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    if (days > 0) {
        snprintf(out, out_size, "%lu天%lu小时", (unsigned long)days, (unsigned long)hours);
    } else if (hours > 0) {
        snprintf(out, out_size, "%lu小时%lu分", (unsigned long)hours, (unsigned long)minutes);
    } else {
        snprintf(out, out_size, "%lu分钟", (unsigned long)minutes);
    }
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(obj, FONT_CN, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_letter_space(obj, 0, 0);
    lv_obj_set_pos(obj, x, y);
    if (w > 0) {
        lv_obj_set_width(obj, w);
    }
    return obj;
}

static lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t accent, lv_color_t paper, const char *title)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_add_style(obj, &s_style_card, 0);
    lv_obj_set_style_bg_color(obj, paper, 0);
    lv_obj_set_style_border_color(obj, accent, 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *tab = lv_obj_create(obj);
    lv_obj_remove_style_all(tab);
    lv_obj_set_style_bg_color(tab, accent, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tab, 4, 0);
    lv_obj_set_pos(tab, 16, -3);
    lv_obj_set_size(tab, 46, 9);

    if (title != NULL && title[0] != '\0') {
        label(obj, title, accent, 16, 14, w - 32);
    }
    return obj;
}

static void add_metric(lv_obj_t *parent, const char *name, const char *value, int x, int y, int w)
{
    label(parent, name, lv_color_hex(0x66706D), x, y, w);
    label(parent, value, lv_color_hex(0x1F2B2A), x, y + 24, w);
}

static void add_chip(lv_obj_t *parent, const char *text, lv_color_t color, int x, int y, int w)
{
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_add_style(chip, &s_style_chip, 0);
    lv_obj_set_style_bg_color(chip, lv_color_mix(color, lv_color_white(), 180), 0);
    lv_obj_set_style_border_color(chip, color, 0);
    lv_obj_set_pos(chip, x, y);
    lv_obj_set_size(chip, w, 34);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *txt = label(chip, text, lv_color_hex(0x22302E), 10, 7, w - 20);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
}

static void add_bar(lv_obj_t *parent, int x, int y, int w, int value, lv_color_t color)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, 16);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF4E7D4), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
}

static void draw_no_data(const char *text)
{
    const page_theme_t *theme = &PAGE_THEMES[s_page];
    lv_obj_t *empty = card(s_content, 252, 166, 520, 170, theme->accent, lv_color_hex(0xFFFDF7), "等待数据");
    label(empty, text, lv_color_hex(0x24302F), 26, 54, 468);
    label(empty, "请确认 NAS 端 Docker Agent 已启动，并且设备能访问该地址。", lv_color_hex(0x6D7774), 26, 88, 468);
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

static void draw_overview(void)
{
    char value[64];
    const page_theme_t *theme = &PAGE_THEMES[PAGE_OVERVIEW];

    lv_obj_t *hero = card(s_content, 24, 24, 436, 220, color_for_health(s_status.nas.health), lv_color_hex(0xFFFDF7), "NAS 状态");
    label(hero, safe_text(s_status.nas.hostname, "NAS 主机"), lv_color_hex(0x1F2B2A), 22, 48, 380);
    add_metric(hero, "IP 地址", safe_text(s_status.nas.primary_ip, "--"), 22, 88, 180);
    format_uptime_cn(s_status.nas.uptime_sec, value, sizeof(value));
    add_metric(hero, "运行时间", value, 220, 88, 170);
    snprintf(value, sizeof(value), "%s / %d 条告警", health_text(s_status.nas.health), s_status.nas.alert_count);
    add_chip(hero, value, color_for_health(s_status.nas.health), 22, 160, 250);

    lv_obj_t *perf = card(s_content, 484, 24, 248, 220, theme->accent, lv_color_hex(0xFFF3ED), "资源");
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    add_metric(perf, "CPU 占用", value, 18, 50, 190);
    add_bar(perf, 18, 106, 190, pct_i(s_status.cpu.usage_pct), lv_color_hex(0xEC6F66));
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    add_metric(perf, "内存占用", value, 18, 134, 190);
    add_bar(perf, 18, 190, 190, pct_i(s_status.memory.used_pct), lv_color_hex(0x249F9C));

    lv_obj_t *storage = card(s_content, 756, 24, 244, 220, lv_color_hex(0xF2B84B), lv_color_hex(0xFFF8E1), "容量");
    if (s_status.storage.pool_count > 0) {
        uint64_t total_sum = 0;
        uint64_t free_sum = 0;
        for (int i = 0; i < s_status.storage.pool_count; ++i) {
            total_sum += s_status.storage.pools[i].total_bytes;
            free_sum += s_status.storage.pools[i].free_bytes;
        }
        float used_pct = total_sum > 0 ? (float)(total_sum - free_sum) / (float)total_sum * 100.0f : 0.0f;
        char total[24];
        char free_b[24];
        nas_format_bytes(total_sum, total, sizeof(total));
        nas_format_bytes(free_sum, free_b, sizeof(free_b));
        format_pct(used_pct, value, sizeof(value));
        add_metric(storage, "已用空间", value, 18, 50, 170);
        add_bar(storage, 18, 106, 184, pct_i(used_pct), lv_color_hex(0xF2B84B));
        add_metric(storage, "总容量 / 剩余", total, 18, 134, 170);
        label(storage, free_b, lv_color_hex(0x1F2B2A), 18, 182, 170);
    } else {
        label(storage, "暂无存储池数据", lv_color_hex(0x66706D), 18, 80, 190);
    }

    lv_obj_t *net = card(s_content, 24, 272, 310, 190, lv_color_hex(0x32A867), lv_color_hex(0xEBF8EC), "网络速率");
    char rx[24];
    char tx[24];
    nas_format_bps(s_status.network.total_rx_bps, rx, sizeof(rx));
    nas_format_bps(s_status.network.total_tx_bps, tx, sizeof(tx));
    add_metric(net, "下载", rx, 18, 50, 120);
    add_metric(net, "上传", tx, 168, 50, 120);
    snprintf(value, sizeof(value), "%d 个网口", s_status.network.interface_count);
    add_chip(net, value, color_for_health(s_status.network.health), 18, 132, 130);

    lv_obj_t *temp = card(s_content, 358, 272, 310, 190, lv_color_hex(0xE08A3D), lv_color_hex(0xFFF2E7), "温度");
    format_temp(s_status.cpu.temperature_c, value, sizeof(value));
    add_metric(temp, "CPU 温度", value, 18, 50, 120);
    if (s_status.drive_count > 0) {
        format_temp(s_status.drives[0].temperature_c, value, sizeof(value));
        add_metric(temp, "首块硬盘", value, 168, 50, 120);
    }
    snprintf(value, sizeof(value), "%d 块硬盘 / %d 块 M.2", s_status.drive_count, s_status.nvme_count);
    add_chip(temp, value, theme->accent, 18, 132, 220);

    lv_obj_t *apps = card(s_content, 692, 272, 308, 190, lv_color_hex(0x8766C9), lv_color_hex(0xF4EEFF), "应用服务");
    snprintf(value, sizeof(value), "%d 运行 / %d 停止", s_status.workloads.docker.running, s_status.workloads.docker.stopped);
    add_metric(apps, "Docker", value, 18, 50, 190);
    snprintf(value, sizeof(value), "备份 %d · 快照 %d", s_status.workloads.backup_count, s_status.workloads.snapshot_count);
    add_metric(apps, "任务", value, 18, 114, 220);
}

static void draw_performance(void)
{
    char value[64];
    lv_obj_t *cpu = card(s_content, 24, 24, 468, 222, PAGE_THEMES[PAGE_PERFORMANCE].accent, lv_color_hex(0xF0FFFA), "CPU");
    format_pct(s_status.cpu.usage_pct, value, sizeof(value));
    add_metric(cpu, "占用率", value, 24, 52, 160);
    add_bar(cpu, 24, 110, 390, pct_i(s_status.cpu.usage_pct), PAGE_THEMES[PAGE_PERFORMANCE].accent);
    format_temp(s_status.cpu.temperature_c, value, sizeof(value));
    add_metric(cpu, "温度", value, 226, 52, 130);
    snprintf(value, sizeof(value), "%d 核心", s_status.cpu.core_count);
    add_metric(cpu, "核心数", value, 24, 146, 120);
    snprintf(value, sizeof(value), "%.2f / %.2f / %.2f", s_status.cpu.load_one, s_status.cpu.load_five, s_status.cpu.load_fifteen);
    add_metric(cpu, "负载 1/5/15 分钟", value, 176, 146, 250);

    lv_obj_t *mem = card(s_content, 526, 24, 474, 222, lv_color_hex(0xEC6F66), lv_color_hex(0xFFF2EE), "内存");
    char total[24];
    char used[24];
    char cache[24];
    nas_format_bytes(s_status.memory.total_bytes, total, sizeof(total));
    nas_format_bytes(s_status.memory.used_bytes, used, sizeof(used));
    nas_format_bytes(s_status.memory.cache_bytes, cache, sizeof(cache));
    format_pct(s_status.memory.used_pct, value, sizeof(value));
    add_metric(mem, "内存占用", value, 24, 52, 150);
    add_bar(mem, 24, 110, 390, pct_i(s_status.memory.used_pct), lv_color_hex(0xEC6F66));
    snprintf(value, sizeof(value), "%s / %s", used, total);
    add_metric(mem, "已用 / 总量", value, 24, 146, 210);
    add_metric(mem, "缓存", cache, 276, 146, 150);

    lv_obj_t *swap = card(s_content, 24, 276, 468, 186, lv_color_hex(0xF2B84B), lv_color_hex(0xFFF7DA), "Swap");
    char swap_total[24];
    char swap_used[24];
    nas_format_bytes(s_status.memory.swap_total_bytes, swap_total, sizeof(swap_total));
    nas_format_bytes(s_status.memory.swap_used_bytes, swap_used, sizeof(swap_used));
    format_pct(s_status.memory.swap_used_pct, value, sizeof(value));
    add_metric(swap, "Swap 占用", value, 24, 54, 150);
    add_bar(swap, 24, 112, 390, pct_i(s_status.memory.swap_used_pct), lv_color_hex(0xF2B84B));
    snprintf(value, sizeof(value), "%s / %s", swap_used, swap_total);
    add_metric(swap, "已用 / 总量", value, 226, 54, 190);

    lv_obj_t *health = card(s_content, 526, 276, 474, 186, color_for_health(s_status.cpu.health), lv_color_hex(0xFFFDF7), "健康状态");
    add_chip(health, health_text(s_status.cpu.health), color_for_health(s_status.cpu.health), 24, 58, 150);
    add_chip(health, health_text(s_status.memory.health), color_for_health(s_status.memory.health), 200, 58, 150);
    label(health, "CPU / 内存状态由 NAS Agent 汇总判断。", lv_color_hex(0x66706D), 24, 116, 390);
}

static void draw_storage(void)
{
    char value[96];
    lv_obj_t *pool_card = card(s_content, 24, 24, 976, 204, PAGE_THEMES[PAGE_STORAGE].accent, lv_color_hex(0xFFF8DF), "存储池 / RAID");
    if (s_status.storage.pool_count == 0) {
        label(pool_card, "暂无存储池数据", lv_color_hex(0x66706D), 24, 72, 400);
    }
    for (int i = 0; i < s_status.storage.pool_count && i < 3; ++i) {
        const nas_pool_t *pool = &s_status.storage.pools[i];
        int x = 24 + i * 310;
        char total[24];
        char free_b[24];
        nas_format_bytes(pool->total_bytes, total, sizeof(total));
        nas_format_bytes(pool->free_bytes, free_b, sizeof(free_b));
        label(pool_card, safe_text(pool->name, pool->id), lv_color_hex(0x1F2B2A), x, 50, 270);
        snprintf(value, sizeof(value), "%s · %s", safe_text(pool->raid_type, "--"), health_text(pool->raid_status));
        label(pool_card, value, lv_color_hex(0x66706D), x, 82, 270);
        format_pct(pool->used_pct, value, sizeof(value));
        add_metric(pool_card, "已用", value, x, 112, 100);
        add_bar(pool_card, x + 92, 124, 166, pct_i(pool->used_pct), PAGE_THEMES[PAGE_STORAGE].accent);
        snprintf(value, sizeof(value), "%s 剩余 %s", total, free_b);
        label(pool_card, value, lv_color_hex(0x1F2B2A), x, 154, 270);
    }

    lv_obj_t *vol_card = card(s_content, 24, 262, 976, 200, lv_color_hex(0x6C8AE4), lv_color_hex(0xEDF3FF), "卷容量");
    if (s_status.storage.volume_count == 0) {
        label(vol_card, "暂无卷数据", lv_color_hex(0x66706D), 24, 72, 400);
    }
    for (int i = 0; i < s_status.storage.volume_count && i < 4; ++i) {
        const nas_volume_t *vol = &s_status.storage.volumes[i];
        int x = 24 + (i % 2) * 470;
        int y = 50 + (i / 2) * 76;
        char used[24];
        char free_b[24];
        nas_format_bytes(vol->used_bytes, used, sizeof(used));
        nas_format_bytes(vol->free_bytes, free_b, sizeof(free_b));
        snprintf(value, sizeof(value), "%s · %s", safe_text(vol->name, vol->id), safe_text(vol->filesystem, "--"));
        label(vol_card, value, lv_color_hex(0x1F2B2A), x, y, 260);
        format_pct(vol->used_pct, value, sizeof(value));
        label(vol_card, value, lv_color_hex(0x1F2B2A), x + 282, y, 70);
        add_bar(vol_card, x, y + 30, 350, pct_i(vol->used_pct), lv_color_hex(0x6C8AE4));
        snprintf(value, sizeof(value), "已用 %s / 剩余 %s", used, free_b);
        label(vol_card, value, lv_color_hex(0x66706D), x, y + 50, 350);
    }
}

static void draw_drives(void)
{
    if (s_status.drive_count == 0) {
        draw_no_data("暂无 HDD / SSD 数据");
        return;
    }

    char value[96];
    for (int i = 0; i < s_status.drive_count && i < 8; ++i) {
        const nas_drive_t *drive = &s_status.drives[i];
        int col = i % 4;
        int row = i / 4;
        int x = 24 + col * 248;
        int y = 24 + row * 224;
        lv_obj_t *c = card(s_content, x, y, 224, 196, color_for_health(drive->health), lv_color_hex(0xF9FBFF), safe_text(drive->bay, drive->id));
        snprintf(value, sizeof(value), "%s · %s", drive_type_text(drive->type), health_text(drive->smart_status));
        label(c, value, lv_color_hex(0x66706D), 16, 48, 180);
        format_temp(drive->temperature_c, value, sizeof(value));
        add_metric(c, "温度", value, 16, 76, 80);
        char cap[24];
        nas_format_bytes(drive->capacity_bytes, cap, sizeof(cap));
        add_metric(c, "容量", cap, 116, 76, 80);
        snprintf(value, sizeof(value), "%d 个坏道", drive->bad_sector_count);
        add_metric(c, "坏道", value, 16, 132, 86);
        snprintf(value, sizeof(value), "%d 小时", drive->power_on_hours);
        add_metric(c, "通电", value, 116, 132, 86);
    }
}

static void draw_nvme(void)
{
    if (s_status.nvme_count == 0) {
        draw_no_data("暂无 M.2 / NVMe 数据");
        return;
    }

    char value[96];
    for (int i = 0; i < s_status.nvme_count && i < 6; ++i) {
        const nas_nvme_t *nvme = &s_status.nvme[i];
        int col = i % 3;
        int row = i / 3;
        int x = 24 + col * 330;
        int y = 24 + row * 224;
        lv_obj_t *c = card(s_content, x, y, 306, 196, color_for_health(nvme->health), lv_color_hex(0xFFF3FA), safe_text(nvme->slot, nvme->id));
        label(c, safe_text(nvme->model, "NVMe 固态"), lv_color_hex(0x1F2B2A), 16, 48, 250);
        char cap[24];
        char used[24];
        nas_format_bytes(nvme->capacity_bytes, cap, sizeof(cap));
        nas_format_bytes(nvme->used_bytes, used, sizeof(used));
        add_metric(c, "容量", cap, 16, 82, 100);
        add_metric(c, "已用", used, 156, 82, 100);
        format_temp(nvme->temperature_c, value, sizeof(value));
        add_metric(c, "温度", value, 16, 138, 80);
        snprintf(value, sizeof(value), "%d%% 磨损", nvme->percentage_used_pct > 0 ? nvme->percentage_used_pct : nvme->wear_pct);
        add_metric(c, "寿命", value, 116, 138, 80);
        add_chip(c, health_text(nvme->cache_state), PAGE_THEMES[PAGE_NVME].accent, 206, 132, 76);
    }
}

static void draw_network(void)
{
    char value[96];
    char rx[24];
    char tx[24];

    lv_obj_t *summary = card(s_content, 24, 24, 976, 132, PAGE_THEMES[PAGE_NETWORK].accent, lv_color_hex(0xECFAEF), "网络总览");
    nas_format_bps(s_status.network.total_rx_bps, rx, sizeof(rx));
    nas_format_bps(s_status.network.total_tx_bps, tx, sizeof(tx));
    add_metric(summary, "下载速度", rx, 28, 52, 160);
    add_metric(summary, "上传速度", tx, 220, 52, 160);
    snprintf(value, sizeof(value), "%d 个网口", s_status.network.interface_count);
    add_metric(summary, "网口数量", value, 412, 52, 150);
    add_chip(summary, health_text(s_status.network.health), color_for_health(s_status.network.health), 610, 58, 120);

    for (int i = 0; i < s_status.network.interface_count && i < 6; ++i) {
        const nas_interface_t *iface = &s_status.network.interfaces[i];
        int col = i % 3;
        int row = i / 3;
        int x = 24 + col * 330;
        int y = 184 + row * 142;
        lv_obj_t *c = card(s_content, x, y, 306, 124, color_for_health(iface->status), lv_color_hex(0xF7FFFA), safe_text(iface->name, "网口"));
        snprintf(value, sizeof(value), "%s · %d Mbps", health_text(iface->status), iface->link_speed_mbps);
        label(c, value, lv_color_hex(0x66706D), 16, 46, 250);
        label(c, safe_text(iface->ip, "--"), lv_color_hex(0x1F2B2A), 16, 72, 250);
        snprintf(value, sizeof(value), "错误 %d/%d · 丢包 %d/%d", iface->rx_errors, iface->tx_errors, iface->rx_dropped, iface->tx_dropped);
        label(c, value, lv_color_hex(0x66706D), 16, 96, 250);
    }
}

static void draw_environment(void)
{
    char value[96];
    lv_obj_t *fans = card(s_content, 24, 24, 472, 214, PAGE_THEMES[PAGE_ENV].accent, lv_color_hex(0xFFF3E8), "风扇");
    if (s_status.environment.fan_count == 0) {
        label(fans, "暂无风扇数据", lv_color_hex(0x66706D), 24, 72, 250);
    }
    for (int i = 0; i < s_status.environment.fan_count && i < 4; ++i) {
        const nas_fan_t *fan = &s_status.environment.fans[i];
        int x = 24 + (i % 2) * 210;
        int y = 54 + (i / 2) * 72;
        snprintf(value, sizeof(value), "%d RPM", fan->speed_rpm);
        add_metric(fans, safe_text(fan->name, "风扇"), value, x, y, 160);
        add_chip(fans, health_text(fan->health), color_for_health(fan->health), x, y + 46, 86);
    }

    lv_obj_t *ups = card(s_content, 528, 24, 472, 214, lv_color_hex(0x32A867), lv_color_hex(0xEDFAF0), "UPS");
    if (s_status.environment.ups.present) {
        add_metric(ups, "状态", health_text(s_status.environment.ups.status), 24, 54, 150);
        snprintf(value, sizeof(value), "%d%%", s_status.environment.ups.battery_pct);
        add_metric(ups, "电量", value, 188, 54, 90);
        snprintf(value, sizeof(value), "%d%%", s_status.environment.ups.load_pct);
        add_metric(ups, "负载", value, 318, 54, 90);
        snprintf(value, sizeof(value), "%d 分钟", s_status.environment.ups.runtime_sec / 60);
        add_metric(ups, "续航", value, 24, 132, 160);
        add_chip(ups, health_text(s_status.environment.ups.health), color_for_health(s_status.environment.ups.health), 188, 138, 120);
    } else {
        label(ups, "未检测到 UPS 数据", lv_color_hex(0x66706D), 24, 72, 250);
    }

    lv_obj_t *alerts = card(s_content, 24, 270, 976, 192, lv_color_hex(0xE65D62), lv_color_hex(0xFFF1F1), "系统告警");
    if (s_status.nas.alert_count == 0) {
        add_chip(alerts, "当前无告警", lv_color_hex(0x38A76B), 24, 70, 150);
    }
    for (int i = 0; i < s_status.nas.alert_count && i < NAS_MAX_ALERTS && i < 4; ++i) {
        const nas_alert_t *alert = &s_status.nas.alerts[i];
        int y = 54 + i * 32;
        snprintf(value, sizeof(value), "%s：%s", health_text(alert->level), safe_text(alert->message, "--"));
        label(alerts, value, color_for_health(alert->level), 24, y, 900);
    }
}

static void draw_apps(void)
{
    char value[96];
    lv_obj_t *summary = card(s_content, 24, 24, 976, 142, PAGE_THEMES[PAGE_APPS].accent, lv_color_hex(0xF5F0FF), "Docker / 任务");
    snprintf(value, sizeof(value), "%d 个运行", s_status.workloads.docker.running);
    add_metric(summary, "运行容器", value, 28, 56, 150);
    snprintf(value, sizeof(value), "%d 个停止", s_status.workloads.docker.stopped);
    add_metric(summary, "停止容器", value, 220, 56, 150);
    snprintf(value, sizeof(value), "%d 个异常", s_status.workloads.docker.unhealthy);
    add_metric(summary, "异常容器", value, 412, 56, 150);
    snprintf(value, sizeof(value), "备份 %d · 快照 %d", s_status.workloads.backup_count, s_status.workloads.snapshot_count);
    add_metric(summary, "备份 / 快照", value, 604, 56, 220);

    lv_obj_t *list = card(s_content, 24, 198, 976, 264, lv_color_hex(0x6C8AE4), lv_color_hex(0xF3F7FF), "服务列表");
    if (s_status.workloads.docker.container_count == 0) {
        label(list, "暂无容器数据", lv_color_hex(0x66706D), 24, 74, 300);
    }
    for (int i = 0; i < s_status.workloads.docker.container_count && i < NAS_MAX_CONTAINERS && i < 8; ++i) {
        const nas_container_t *container = &s_status.workloads.docker.containers[i];
        int col = i % 2;
        int row = i / 2;
        int x = 24 + col * 460;
        int y = 54 + row * 48;
        label(list, safe_text(container->name, "容器"), lv_color_hex(0x1F2B2A), x, y, 260);
        snprintf(value, sizeof(value), "%s / %s", health_text(container->state), health_text(container->health));
        add_chip(list, value, color_for_health(container->health), x + 280, y - 4, 130);
    }
}

static bool valid_endpoint_host(const char *host)
{
    if (host == NULL || host[0] == '\0' || strlen(host) >= sizeof(s_endpoint_host)) {
        return false;
    }
    for (const char *p = host; *p != '\0'; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (isspace(ch) || ch == '/' || ch == ':' || ch == '?' || ch == '#') {
            return false;
        }
    }
    return true;
}

static int parse_port(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    char *end = NULL;
    long port = strtol(text, &end, 10);
    if (end == text || *end != '\0' || port <= 0 || port > 65535) {
        return -1;
    }
    return (int)port;
}

static void set_settings_hint(const char *text, lv_color_t color)
{
    if (s_settings_hint == NULL) {
        return;
    }
    lv_label_set_text(s_settings_hint, text);
    lv_obj_set_style_text_color(s_settings_hint, color, 0);
}

static void keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(lv_event_get_target(event), LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_keyboard_for(lv_obj_t *textarea)
{
    if (s_keyboard == NULL || textarea == NULL) {
        return;
    }

    lv_keyboard_set_textarea(s_keyboard, textarea);
    if (textarea == s_port_ta) {
        lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_NUMBER);
    } else {
        lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    }
    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
    ESP_LOGI(TAG, "keyboard shown for %s", textarea == s_port_ta ? "port" : "host");
}

static void textarea_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        show_keyboard_for(lv_event_get_target(event));
    }
}

static void input_hotspot_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) {
        return;
    }

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
    if (s_keyboard != NULL && lv_obj_is_valid(s_keyboard)) {
        return;
    }

    s_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(s_keyboard, 900, 220);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_text_font(s_keyboard, &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_color(s_keyboard, lv_color_hex(0xF6FBFF), 0);
    lv_obj_set_style_bg_opa(s_keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_keyboard, 3, 0);
    lv_obj_set_style_border_color(s_keyboard, lv_color_hex(0x4C96D7), 0);
    lv_obj_add_event_cb(s_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void save_settings_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || s_host_ta == NULL || s_port_ta == NULL) {
        return;
    }

    const char *host = lv_textarea_get_text(s_host_ta);
    int port = parse_port(lv_textarea_get_text(s_port_ta));
    if (!valid_endpoint_host(host) || port < 0) {
        set_settings_hint("地址或端口格式不正确", lv_color_hex(0xE65D62));
        return;
    }

    bool saved = s_save_cb != NULL ? s_save_cb(host, port, s_save_user_data) : false;
    if (!saved) {
        set_settings_hint("保存失败，请稍后重试", lv_color_hex(0xE65D62));
        return;
    }

    strlcpy(s_endpoint_host, host, sizeof(s_endpoint_host));
    s_endpoint_port = port;
    strlcpy(s_message, "接口地址已保存", sizeof(s_message));
    set_settings_hint("已保存，下一次刷新会使用新地址", lv_color_hex(0x38A76B));
    if (s_keyboard != NULL) {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_state(s_host_ta, LV_STATE_FOCUSED);
    lv_obj_clear_state(s_port_ta, LV_STATE_FOCUSED);
    lv_obj_invalidate(s_header);
}

static void draw_settings(void)
{
    char value[24];
    ensure_keyboard();

    lv_obj_t *panel = card(s_content, 104, 34, 816, 274, PAGE_THEMES[PAGE_SETTINGS].accent, lv_color_hex(0xF1FAFF), "NAS Agent 地址");
    label(panel, "主机地址", lv_color_hex(0x40504E), 30, 58, 120);
    s_host_ta = lv_textarea_create(panel);
    lv_obj_add_style(s_host_ta, &s_style_textarea, 0);
    lv_obj_set_pos(s_host_ta, 148, 44);
    lv_obj_set_size(s_host_ta, 430, 52);
    lv_textarea_set_one_line(s_host_ta, true);
    lv_textarea_set_max_length(s_host_ta, sizeof(s_endpoint_host) - 1);
    lv_textarea_set_accepted_chars(s_host_ta, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_");
    lv_textarea_set_placeholder_text(s_host_ta, "IP 或域名");
    lv_textarea_set_text(s_host_ta, s_endpoint_host);
    lv_obj_add_flag(s_host_ta, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_host_ta, textarea_event_cb, LV_EVENT_ALL, NULL);

    label(panel, "端口", lv_color_hex(0x40504E), 30, 128, 120);
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
    lv_obj_set_style_bg_color(save, PAGE_THEMES[PAGE_SETTINGS].accent, 0);
    lv_obj_set_pos(save, 616, 70);
    lv_obj_set_size(save, 150, 70);
    lv_obj_add_event_cb(save, save_settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_text = label(save, "保存", lv_color_white(), 0, 23, 150);
    lv_obj_set_style_text_align(save_text, LV_TEXT_ALIGN_CENTER, 0);

    s_settings_hint = label(panel, "支持域名或 IPv4 地址，端口范围 1-65535", lv_color_hex(0x66706D), 30, 206, 720);

    lv_obj_t *current = card(s_content, 104, 338, 816, 74, lv_color_hex(0x38A76B), lv_color_hex(0xF4FFF7), "当前连接");
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "%s:%d", s_endpoint_host, s_endpoint_port);
    label(current, endpoint, lv_color_hex(0x1F2B2A), 24, 38, 520);
    add_chip(current, s_online ? "在线" : "离线", s_online ? lv_color_hex(0x38A76B) : lv_color_hex(0xE65D62), 638, 26, 110);

    add_input_hotspot(panel, s_host_ta, 148, 44, 430, 52);
    add_input_hotspot(panel, s_port_ta, 148, 114, 160, 52);

    lv_keyboard_set_textarea(s_keyboard, s_host_ta);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
}

static void draw_page(void)
{
    clear_content();
    const page_theme_t *theme = &PAGE_THEMES[s_page];
    lv_obj_set_style_bg_color(s_content, theme->paper, 0);

    if (!s_has_status && s_page != PAGE_SETTINGS) {
        draw_no_data("还没有收到 NAS 状态数据");
        return;
    }

    switch (s_page) {
    case PAGE_OVERVIEW:
        draw_overview();
        break;
    case PAGE_PERFORMANCE:
        draw_performance();
        break;
    case PAGE_STORAGE:
        draw_storage();
        break;
    case PAGE_DRIVES:
        draw_drives();
        break;
    case PAGE_NVME:
        draw_nvme();
        break;
    case PAGE_NETWORK:
        draw_network();
        break;
    case PAGE_ENV:
        draw_environment();
        break;
    case PAGE_APPS:
        draw_apps();
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
    const page_theme_t *theme = &PAGE_THEMES[s_page];
    lv_obj_set_style_bg_color(s_header, lv_color_mix(theme->accent, lv_color_white(), 48), 0);
    lv_obj_set_style_border_color(s_header, theme->accent, 0);
    lv_label_set_text(s_page_label, theme->name);

    char endpoint[200];
    snprintf(endpoint, sizeof(endpoint), "%s:%d  %s", s_endpoint_host, s_endpoint_port, s_message);
    lv_label_set_text(s_endpoint_label, endpoint);

    lv_label_set_text(s_status_label, s_online ? "在线" : "离线");
    lv_obj_set_style_bg_color(s_status_label, s_online ? lv_color_hex(0x38A76B) : lv_color_hex(0xE65D62), 0);

    char page_index[32];
    snprintf(page_index, sizeof(page_index), "第 %d/%d 页", (int)s_page + 1, PAGE_COUNT);
    lv_label_set_text(s_page_index_label, page_index);
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
    if (s_keyboard != NULL && !lv_obj_has_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

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
    lv_style_set_bg_color(&s_style_screen, lv_color_hex(0xFFF3DF));
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);

    lv_style_init(&s_style_header);
    lv_style_set_bg_color(&s_style_header, lv_color_hex(0xFFEBCA));
    lv_style_set_bg_opa(&s_style_header, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_header, 0);
    lv_style_set_border_side(&s_style_header, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&s_style_header, lv_color_hex(0xEC6F66));
    lv_style_set_pad_all(&s_style_header, 0);

    lv_style_init(&s_style_card);
    lv_style_set_bg_color(&s_style_card, lv_color_hex(0xFFFDF7));
    lv_style_set_bg_opa(&s_style_card, LV_OPA_COVER);
    lv_style_set_radius(&s_style_card, 8);
    lv_style_set_border_width(&s_style_card, 3);
    lv_style_set_pad_all(&s_style_card, 0);
    lv_style_set_shadow_width(&s_style_card, 6);
    lv_style_set_shadow_ofs_x(&s_style_card, 2);
    lv_style_set_shadow_ofs_y(&s_style_card, 3);
    lv_style_set_shadow_color(&s_style_card, lv_color_hex(0xD4C6B6));

    lv_style_init(&s_style_chip);
    lv_style_set_bg_opa(&s_style_chip, LV_OPA_COVER);
    lv_style_set_radius(&s_style_chip, 8);
    lv_style_set_border_width(&s_style_chip, 2);
    lv_style_set_pad_all(&s_style_chip, 0);

    lv_style_init(&s_style_textarea);
    lv_style_set_bg_color(&s_style_textarea, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&s_style_textarea, LV_OPA_COVER);
    lv_style_set_radius(&s_style_textarea, 8);
    lv_style_set_border_width(&s_style_textarea, 3);
    lv_style_set_border_color(&s_style_textarea, lv_color_hex(0x4C96D7));
    lv_style_set_pad_left(&s_style_textarea, 12);
    lv_style_set_pad_top(&s_style_textarea, 10);
    lv_style_set_text_font(&s_style_textarea, FONT_CN);
    lv_style_set_text_color(&s_style_textarea, lv_color_hex(0x1F2B2A));

    lv_style_init(&s_style_button);
    lv_style_set_radius(&s_style_button, 8);
    lv_style_set_border_width(&s_style_button, 3);
    lv_style_set_border_color(&s_style_button, lv_color_hex(0x1F2B2A));
    lv_style_set_shadow_width(&s_style_button, 5);
    lv_style_set_shadow_ofs_x(&s_style_button, 2);
    lv_style_set_shadow_ofs_y(&s_style_button, 3);
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

    s_page_label = label(s_header, PAGE_THEMES[s_page].name, lv_color_hex(0x1F2B2A), 24, 16, 120);
    s_endpoint_label = label(s_header, "", lv_color_hex(0x40504E), 160, 14, 560);
    s_status_label = label(s_header, "离线", lv_color_white(), 0, 0, 82);
    lv_obj_add_style(s_status_label, &s_style_chip, 0);
    lv_obj_set_style_bg_color(s_status_label, lv_color_hex(0xE65D62), 0);
    lv_obj_set_style_border_color(s_status_label, lv_color_hex(0x1F2B2A), 0);
    lv_obj_set_pos(s_status_label, 806, 17);
    lv_obj_set_size(s_status_label, 82, 36);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);

    s_page_index_label = label(s_header, "", lv_color_hex(0x40504E), 908, 24, 92);

    s_content = lv_obj_create(screen);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_pos(s_content, 0, CONTENT_Y);
    lv_obj_set_size(s_content, SCREEN_W, CONTENT_H);
    lv_obj_set_style_bg_color(s_content, PAGE_THEMES[s_page].paper, 0);
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
    if ((!s_has_status || s_page == PAGE_SETTINGS) &&
        (s_page != PAGE_SETTINGS || s_keyboard == NULL || lv_obj_has_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN))) {
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
    strlcpy(s_message, online ? "数据已刷新" : "连接失败", sizeof(s_message));
    refresh_header();
    if (s_page != PAGE_SETTINGS || s_keyboard == NULL || lv_obj_has_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN)) {
        draw_page();
    }
    board_lvgl_unlock();
}
