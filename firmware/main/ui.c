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

/* ── page enum ───────────────────────────────────── */

typedef enum {
    PAGE_HOME = 0,
    PAGE_PERFORMANCE,
    PAGE_STORAGE,
    PAGE_DRIVES,
    PAGE_NVME,
    PAGE_NETWORK,
    PAGE_APPS,
    PAGE_SETTINGS,
    PAGE_COUNT,
} ui_page_t;

/* ── design system ───────────────────────────────── */

#define SCREEN_W  1024
#define SCREEN_H  600
#define HEADER_H  58
#define NAV_H     62
#define CONTENT_Y HEADER_H
#define CONTENT_H (SCREEN_H - HEADER_H - NAV_H)
#define GAP 14
#define RADIUS_CARD 18

LV_FONT_DECLARE(lv_font_nas_cn_18);
#define FONT_CN       (&lv_font_nas_cn_18)
#define FONT_BIG      (&lv_font_montserrat_24)
#define FONT_NUM      (&lv_font_montserrat_20)
#define FONT_SMALL    (&lv_font_montserrat_14)

static const lv_color_t C_BG          = LV_COLOR_MAKE(0xFF, 0xF8, 0xEE);
static const lv_color_t C_CARD        = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
static const lv_color_t C_SHADOW      = LV_COLOR_MAKE(0xE0, 0xD3, 0xC2);
static const lv_color_t C_TEXT        = LV_COLOR_MAKE(0x2C, 0x3E, 0x50);
static const lv_color_t C_SUBTEXT     = LV_COLOR_MAKE(0x7F, 0x8C, 0x8D);
static const lv_color_t C_MUTED       = LV_COLOR_MAKE(0xBD, 0xC3, 0xC7);
static const lv_color_t C_BAR_BG      = LV_COLOR_MAKE(0xF0, 0xEB, 0xE6);
static const lv_color_t C_WHITE       = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
static const lv_color_t C_GREEN       = LV_COLOR_MAKE(0x27, 0xAE, 0x60);
static const lv_color_t C_RED         = LV_COLOR_MAKE(0xE7, 0x4C, 0x3C);
static const lv_color_t C_AMBER       = LV_COLOR_MAKE(0xF3, 0x9C, 0x12);
static const lv_color_t C_GRAY        = LV_COLOR_MAKE(0x95, 0xA5, 0xA6);
static const lv_color_t C_BLUE        = LV_COLOR_MAKE(0x34, 0x98, 0xDB);
static const lv_color_t C_PURPLE      = LV_COLOR_MAKE(0x9B, 0x59, 0xB6);
static const lv_color_t C_TEAL        = LV_COLOR_MAKE(0x1A, 0xBC, 0x9C);
static const lv_color_t C_ORANGE      = LV_COLOR_MAKE(0xE6, 0x7E, 0x22);

static const char *PAGE_NAMES[PAGE_COUNT] = {
    "\u603B\u89C8", "\u6027\u80FD", "\u5B58\u50A8", "\u786C\u76D8",
    "M.2", "\u7F51\u7EDC", "\u5E94\u7528", "\u8BBE\u7F6E",
};
static const lv_color_t PAGE_COLORS[PAGE_COUNT] = {
    [PAGE_HOME]        = C_RED,
    [PAGE_PERFORMANCE] = C_BLUE,
    [PAGE_STORAGE]     = C_AMBER,
    [PAGE_DRIVES]      = C_GREEN,
    [PAGE_NVME]        = C_PURPLE,
    [PAGE_NETWORK]     = C_TEAL,
    [PAGE_APPS]        = C_ORANGE,
    [PAGE_SETTINGS]    = C_GRAY,
};
static const lv_color_t PAGE_LIGHT[PAGE_COUNT] = {
    [PAGE_HOME]        = LV_COLOR_MAKE(0xFD, 0xED, 0xEC),
    [PAGE_PERFORMANCE] = LV_COLOR_MAKE(0xEB, 0xF5, 0xFB),
    [PAGE_STORAGE]     = LV_COLOR_MAKE(0xFE, 0xF5, 0xE7),
    [PAGE_DRIVES]      = LV_COLOR_MAKE(0xE9, 0xF7, 0xEF),
    [PAGE_NVME]        = LV_COLOR_MAKE(0xF4, 0xEC, 0xF7),
    [PAGE_NETWORK]     = LV_COLOR_MAKE(0xE8, 0xF8, 0xF5),
    [PAGE_APPS]        = LV_COLOR_MAKE(0xFD, 0xF2, 0xE5),
    [PAGE_SETTINGS]    = LV_COLOR_MAKE(0xF2, 0xF4, 0xF4),
};

static const lv_color_t C_NAV_BG __attribute__((unused)) = LV_COLOR_MAKE(0xE8, 0xF0, 0xFE);
static const lv_color_t C_CHIP_BG     = LV_COLOR_MAKE(0xF9, 0xF5, 0xED);
static const lv_color_t C_TA_BG       = LV_COLOR_MAKE(0xFA, 0xF7, 0xF2);
static const lv_color_t C_TA_BORDER   = LV_COLOR_MAKE(0xDC, 0xD0, 0xC0);
static const lv_color_t C_CARD_BORDER = LV_COLOR_MAKE(0xE8, 0xDC, 0xCC);
static const lv_color_t C_NAV_WHITE   = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);

static lv_obj_t *s_scr;
static lv_obj_t *s_header;
static lv_obj_t *s_content;
static lv_obj_t *s_nav;
static lv_obj_t *s_nav_btns[PAGE_COUNT];
static lv_obj_t *s_nav_dots[PAGE_COUNT];
static lv_obj_t *s_page_label;
static lv_obj_t *s_status_chip;
static lv_obj_t *s_endpoint_label;
static lv_obj_t *s_host_ta;
static lv_obj_t *s_port_ta;
static lv_obj_t *s_keyboard;
static lv_obj_t *s_settings_hint;

static ui_page_t s_page = PAGE_HOME;
static nas_status_t s_status;
static bool s_has_status;
static bool s_online;
static char s_msg[96] = "\u6B63\u5728\u542F\u52A8";
static char s_host[64] = CONFIG_NAS_DISPLAY_API_HOST;
static int  s_port = CONFIG_NAS_DISPLAY_API_PORT;
static ui_endpoint_save_cb_t s_save_cb;
static void *s_save_user_data;

static lv_style_t sty_screen, sty_card, sty_chip, sty_bar_bg, sty_bar_ind;
static lv_style_t sty_ta, sty_btn;

/* ── helpers ─────────────────────────────────────── */

static const char *s(const char *t, const char *fb) { return t && *t ? t : fb; }
static bool eq(const char *a, const char *b) { return a && strcasecmp(a, b) == 0; }

static const char *ht(const char *v)
{
    if (!v || !*v || eq(v, "unknown")) return "\u672A\u77E5";
    if (eq(v, "ok")||eq(v, "good")||eq(v, "healthy")||eq(v, "passed")||eq(v, "clean")||eq(v, "active")||eq(v, "online")||eq(v, "up")) return "\u6B63\u5E38";
    if (eq(v, "warning")||eq(v, "warn")||eq(v, "degraded")) return "\u8B66\u544A";
    if (eq(v, "critical")||eq(v, "error")||eq(v, "failed")||eq(v, "unhealthy")||eq(v, "down")) return "\u5F02\u5E38";
    if (eq(v, "running"))  return "\u8FD0\u884C\u4E2D";
    if (eq(v, "stopped")||eq(v, "exited")) return "\u5DF2\u505C\u6B62";
    if (eq(v, "resync")||eq(v, "syncing")) return "\u540C\u6B65\u4E2D";
    if (eq(v, "rebuild")||eq(v, "rebuilding")||eq(v, "repairing")) return "\u91CD\u5EFA\u4E2D";
    if (eq(v, "checking")||eq(v, "check")) return "\u68C0\u67E5\u4E2D";
    if (eq(v, "enabled"))  return "\u5DF2\u542F\u7528";
    if (eq(v, "disabled")||eq(v, "inactive")) return "\u672A\u542F\u7528";
    if (eq(v, "idle"))     return "\u7A7A\u95F2";
    if (eq(v, "missing"))  return "\u7F3A\u5931";
    if (eq(v, "disconnected")) return "\u672A\u63A5\u5165";
    return "\u672A\u77E5";
}

static const char *dt(const char *t)
{
    if (eq(t, "hdd"))  return "\u673A\u68B0\u76D8";
    if (eq(t, "ssd"))  return "\u56FA\u6001\u76D8";
    if (eq(t, "nvme")) return "NVMe";
    return s(t, "\u786C\u76D8");
}

static lv_color_t hc(const char *h)
{
    if (!h||!*h) return C_GRAY;
    if (eq(h,"critical")||eq(h,"failed")||eq(h,"unhealthy")||eq(h,"error")||eq(h,"down")) return C_RED;
    if (eq(h,"warning")||eq(h,"warn")||eq(h,"degraded")) return C_AMBER;
    if (eq(h,"ok")||eq(h,"good")||eq(h,"healthy")||eq(h,"passed")||eq(h,"clean")||eq(h,"online")||eq(h,"up")||eq(h,"running")) return C_GREEN;
    return C_GRAY;
}

static lv_color_t sc(const char *st)
{
    if (!st) return C_GRAY;
    if (eq(st,"up")||eq(st,"online")) return C_GREEN;
    if (eq(st,"down")||eq(st,"disconnected")) return C_MUTED;
    return C_GRAY;
}

static int pct_i(float v) { return v<0?0:v>100?100:(int)(v+0.5f); }
static void fmt_pct(float v, char *o, size_t sz) {
    if (v<0) snprintf(o,sz,"--"); else snprintf(o,sz,"%.0f%%",v);
}
static void fmt_temp(float v, char *o, size_t sz) {
    if (v<=0) snprintf(o,sz,"--"); else snprintf(o,sz,"%.0f\u2103",v);
}
static void fmt_up(uint32_t s, char *o, size_t sz) {
    uint32_t d=s/86400,h=(s%86400)/3600,m=(s%3600)/60;
    if (d) snprintf(o,sz,"%lu\u5929%lu\u65F6",(unsigned long)d,(unsigned long)h);
    else if (h) snprintf(o,sz,"%lu\u65F6%lu\u5206",(unsigned long)h,(unsigned long)m);
    else snprintf(o,sz,"%lu\u5206",(unsigned long)m);
}

static lv_obj_t *lbl(lv_obj_t *p, const char *t, lv_color_t c, int x, int y, int w)
{
    lv_obj_t *o=lv_label_create(p);
    lv_label_set_text(o,t); lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(o,FONT_CN,0); lv_obj_set_style_text_color(o,c,0);
    lv_obj_set_style_text_letter_space(o,0,0); lv_obj_set_pos(o,x,y);
    if (w>0) lv_obj_set_width(o,w);
    return o;
}

static lv_obj_t *lbl_sm(lv_obj_t *p, const char *t, lv_color_t c, int x, int y, int w)
{
    lv_obj_t *o=lbl(p,t,c,x,y,w);
    lv_obj_set_style_text_font(o,FONT_SMALL,0);
    return o;
}

static lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h, lv_color_t accent, const char *title)
{
    lv_obj_t *o=lv_obj_create(p);
    lv_obj_add_style(o,&sty_card,0);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (title && *title) {
        lv_obj_t *line=lv_obj_create(o); lv_obj_remove_style_all(line);
        lv_obj_set_style_bg_color(line,accent,0); lv_obj_set_style_bg_opa(line,LV_OPA_COVER,0);
        lv_obj_set_style_radius(line,2,0); lv_obj_set_pos(line,18,6); lv_obj_set_size(line,32,4);
        lbl(o,title,C_TEXT,18,20,w-36);
    }
    return o;
}

static void chip(lv_obj_t *p, const char *t, lv_color_t c, int x, int y, int w)
{
    lv_obj_t *o=lv_obj_create(p); lv_obj_add_style(o,&sty_chip,0);
    lv_obj_set_style_border_color(o,c,0); lv_obj_set_pos(o,x,y);
    lv_obj_set_size(o,w,30); lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *tx=lbl(o,t,c,8,5,w-16);
    lv_obj_set_style_text_align(tx,LV_TEXT_ALIGN_CENTER,0);
}

static void bar(lv_obj_t *p, int x, int y, int w, int v, lv_color_t c)
{
    lv_obj_t *b=lv_bar_create(p); lv_obj_set_pos(b,x,y); lv_obj_set_size(b,w,12);
    lv_bar_set_range(b,0,100); lv_bar_set_value(b,v,LV_ANIM_OFF);
    lv_obj_add_style(b,&sty_bar_bg,LV_PART_MAIN);
    lv_obj_add_style(b,&sty_bar_ind,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(b,c,LV_PART_INDICATOR);
}

static void kpi(lv_obj_t *p, const char *lb, const char *val, int x, int y, int w)
{
    lbl_sm(p,lb,C_SUBTEXT,x,y,w);
    lbl(p,val,C_TEXT,x,y+18,w);
}

static void kpi_temp(lv_obj_t *p, const char *lb, float t, int x, int y, int w)
{
    char b[16]; fmt_temp(t,b,sizeof(b)); kpi(p,lb,b,x,y,w);
}

static void dot(lv_obj_t *p, const char *st, int x, int y)
{
    lv_obj_t *d=lv_obj_create(p); lv_obj_remove_style_all(d);
    lv_obj_set_style_bg_color(d,sc(st),0); lv_obj_set_style_bg_opa(d,LV_OPA_COVER,0);
    lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0);
    lv_obj_set_pos(d,x,y); lv_obj_set_size(d,10,10);
}

/* ── ring (arc) chart ────────────────────────────── */

static lv_obj_t *ring_create(lv_obj_t *p, int x, int y, int sz, lv_color_t c)
{
    lv_obj_t *a=lv_arc_create(p);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a,sz,sz); lv_obj_set_pos(a,x,y);
    lv_obj_set_style_arc_color(a,c,LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a,14,LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a,true,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a,C_BAR_BG,LV_PART_MAIN);
    lv_obj_set_style_arc_width(a,14,LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a,true,LV_PART_MAIN);
    lv_arc_set_range(a,0,100);
    lv_arc_set_bg_angles(a,0,360);
    lv_arc_set_rotation(a,270);
    return a;
}

static void ring_set(lv_obj_t *a, int v) { lv_arc_set_value(a, v); }

/* ── line chart ──────────────────────────────────── */

#define CHART_PTS 60

__attribute__((unused)) static lv_obj_t *chart_create(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *c=lv_chart_create(p);
    lv_obj_set_pos(c,x,y); lv_obj_set_size(c,w,h);
    lv_chart_set_type(c,LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(c,CHART_PTS);
    lv_chart_set_range(c,LV_CHART_AXIS_PRIMARY_Y,0,100);
    lv_chart_set_div_line_count(c,3,3);
    lv_obj_set_style_bg_color(c,C_CARD,0);
    lv_obj_set_style_bg_opa(c,LV_OPA_COVER,0);
    lv_obj_set_style_line_color(c,C_BAR_BG,LV_PART_ITEMS);
    lv_obj_set_style_border_width(c,0,0);
    lv_obj_set_style_radius(c,10,0);
    lv_chart_series_t *s1=lv_chart_add_series(c,C_BLUE,LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_series_t *s2=lv_chart_add_series(c,C_TEAL,LV_CHART_AXIS_PRIMARY_Y);
    for (int i=0;i<CHART_PTS;i++) {
        lv_chart_set_next_value(c,s1,0);
        lv_chart_set_next_value(c,s2,0);
    }
    return c;
}

/* ── page: no data ───────────────────────────────── */

static void draw_empty(const char *msg)
{
    lv_obj_t *ec=card(s_content,212,140,600,200,PAGE_COLORS[s_page],"\u7B49\u5F85\u6570\u636E");
    lbl(ec,msg,C_TEXT,24,60,552);
    lbl(ec,"\u786E\u8BA4 NAS Agent \u5DF2\u542F\u52A8\uFF0C\u4E14\u8BBE\u5907\u53EF\u8BBF\u95EE\u8BE5\u5730\u5740\u3002",C_SUBTEXT,24,92,552);
}

static void clear_content(void)
{
    lv_obj_clean(s_content);
    s_host_ta=NULL; s_port_ta=NULL; s_settings_hint=NULL;
    if (s_keyboard && lv_obj_is_valid(s_keyboard)) {
        lv_keyboard_set_textarea(s_keyboard,NULL);
        lv_obj_add_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN);
    }
}

/* ── HOME ────────────────────────────────────────── */

static void draw_home(void)
{
    char v[96],b[48];

    /* hero: NAS identity */
    lv_obj_t *hero=card(s_content,24,18,420,224,hc(s_status.nas.health),"\u6211\u7684 NAS");
    lbl(hero,s(s_status.nas.hostname,"NAS \u4E3B\u673A"),C_TEXT,20,52,370);
    dot(hero,s_status.nas.health,20,74);
    snprintf(v,sizeof(v),"%s  \u00B7  %s",ht(s_status.nas.health),s(s_status.nas.primary_ip,"--"));
    lbl(hero,v,C_SUBTEXT,36,68,350);
    fmt_up(s_status.nas.uptime_sec,v,sizeof(v));
    kpi(hero,"\u8FD0\u884C\u65F6\u95F4",v,20,106,180);
    snprintf(v,sizeof(v),"%d \u6761\u544A\u8B66",s_status.nas.alert_count);
    chip(hero,v,s_status.nas.alert_count?C_RED:C_GREEN,20,166,150);

    /* resource: CPU + memory */
    lv_obj_t *res=card(s_content,468,18,252,224,C_BLUE,"\u8D44\u6E90");
    fmt_pct(s_status.cpu.usage_pct,v,sizeof(v)); kpi(res,"CPU \u5360\u7528",v,18,50,100);
    bar(res,18,96,194,pct_i(s_status.cpu.usage_pct),C_BLUE);
    fmt_pct(s_status.memory.used_pct,v,sizeof(v)); kpi(res,"\u5185\u5B58\u5360\u7528",v,18,134,100);
    bar(res,18,178,194,pct_i(s_status.memory.used_pct),C_PURPLE);

    /* storage ring */
    lv_obj_t *s_card=card(s_content,744,18,256,224,C_AMBER,"\u5B58\u50A8\u5BB9\u91CF");
    if (s_status.storage.pool_count>0) {
        uint64_t ts=0,fs=0;
        for (int i=0;i<s_status.storage.pool_count;i++) { ts+=s_status.storage.pools[i].total_bytes; fs+=s_status.storage.pools[i].free_bytes; }
        float up=ts?(float)(ts-fs)/(float)ts*100.0f:0;
        lv_obj_t *ring=ring_create(s_card,28,54,160,C_AMBER);
        ring_set(ring,pct_i(up));
        char t_str[24],f_str[24];
        nas_format_bytes(ts,t_str,sizeof(t_str)); nas_format_bytes(fs,f_str,sizeof(f_str));
        lbl(s_card,f_str,C_TEXT,62,114,120); lv_obj_set_style_text_align(lv_obj_get_child(s_card,lv_obj_get_child_cnt(s_card)-1),LV_TEXT_ALIGN_CENTER,0);
        lbl(s_card,"\u5269\u4F59",C_SUBTEXT,72,138,100); lv_obj_set_style_text_align(lv_obj_get_child(s_card,lv_obj_get_child_cnt(s_card)-1),LV_TEXT_ALIGN_CENTER,0);
        fmt_pct(up,v,sizeof(v));
        lbl(s_card,v,C_TEXT,66,168,60); lv_obj_set_style_text_align(lv_obj_get_child(s_card,lv_obj_get_child_cnt(s_card)-1),LV_TEXT_ALIGN_CENTER,0);
        snprintf(b,sizeof(b),"\u603B\u5BB9\u91CF %s",t_str);
        lbl(s_card,b,C_SUBTEXT,18,196,200);
    } else lbl(s_card,"\u6682\u65E0\u5B58\u50A8\u6C60",C_SUBTEXT,18,90,200);

    /* bottom row */
    lv_obj_t *net=card(s_content,24,266,312,180,C_TEAL,"\u7F51\u7EDC\u901F\u7387");
    nas_format_bps(s_status.network.total_rx_bps,b,sizeof(b));
    kpi(net,"\u4E0B\u8F7D",b,18,50,130);
    nas_format_bps(s_status.network.total_tx_bps,b,sizeof(b));
    kpi(net,"\u4E0A\u4F20",b,170,50,130);
    snprintf(v,sizeof(v),"%d \u4E2A\u7F51\u53E3",s_status.network.interface_count);
    chip(net,v,C_TEAL,18,134,130);

    lv_obj_t *temp=card(s_content,360,266,312,180,C_GREEN,"\u6E29\u5EA6\u4E0E\u78C1\u76D8");
    kpi_temp(temp,"CPU \u6E29\u5EA6",s_status.cpu.temperature_c,18,50,130);
    if (s_status.drive_count>0)
        kpi_temp(temp,"\u786C\u76D8\u6E29\u5EA6",s_status.drives[0].temperature_c,170,50,130);
    snprintf(v,sizeof(v),"%d \u5757 HDD/SSD  \u00B7  %d \u5757 M.2",s_status.drive_count,s_status.nvme_count);
    chip(temp,v,C_GREEN,18,134,240);

    lv_obj_t *apps=card(s_content,696,266,304,180,C_ORANGE,"\u5E94\u7528\u670D\u52A1");
    snprintf(v,sizeof(v),"%d \u8FD0\u884C / %d \u505C\u6B62",s_status.workloads.docker.running,s_status.workloads.docker.stopped);
    kpi(apps,"Docker",v,18,50,200);
    snprintf(v,sizeof(v),"\u5907\u4EFD %d \u00B7 \u5FEB\u7167 %d",s_status.workloads.backup_count,s_status.workloads.snapshot_count);
    kpi(apps,"\u4EFB\u52A1",v,18,116,200);
}

/* ── PERFORMANCE ──────────────────────────────────── */

static void draw_performance(void)
{
    char v[64];

    lv_obj_t *cpu=card(s_content,24,18,464,262,C_BLUE,"CPU \u4F7F\u7528\u7387");
    lv_obj_t *ring=ring_create(cpu,28,54,178,C_BLUE);
    ring_set(ring,pct_i(s_status.cpu.usage_pct));
    fmt_pct(s_status.cpu.usage_pct,v,sizeof(v));
    lbl(cpu,v,C_TEXT,92,114,60); lv_obj_set_style_text_align(lv_obj_get_child(cpu,lv_obj_get_child_cnt(cpu)-1),LV_TEXT_ALIGN_CENTER,0);
    lbl_sm(cpu,"CPU %",C_SUBTEXT,90,140,60); lv_obj_set_style_text_align(lv_obj_get_child(cpu,lv_obj_get_child_cnt(cpu)-1),LV_TEXT_ALIGN_CENTER,0);
    fmt_temp(s_status.cpu.temperature_c,v,sizeof(v)); kpi(cpu,"\u6E29\u5EA6",v,226,54,100);
    snprintf(v,sizeof(v),"%d \u6838\u5FC3",s_status.cpu.core_count); kpi(cpu,"\u6838\u5FC3\u6570",v,226,104,100);
    snprintf(v,sizeof(v),"%.2f / %.2f / %.2f",s_status.cpu.load_one,s_status.cpu.load_five,s_status.cpu.load_fifteen);
    kpi(cpu,"\u8D1F\u8F7D 1/5/15 min",v,24,200,400);

    lv_obj_t *mem=card(s_content,512,18,488,262,C_PURPLE,"\u5185\u5B58");
    char tot[24],used[24],cache[24];
    nas_format_bytes(s_status.memory.total_bytes,tot,sizeof(tot));
    nas_format_bytes(s_status.memory.used_bytes,used,sizeof(used));
    nas_format_bytes(s_status.memory.cache_bytes,cache,sizeof(cache));
    fmt_pct(s_status.memory.used_pct,v,sizeof(v)); kpi(mem,"\u5185\u5B58\u5360\u7528",v,20,54,130);
    bar(mem,20,104,420,pct_i(s_status.memory.used_pct),C_PURPLE);
    snprintf(v,sizeof(v),"%s / %s",used,tot); kpi(mem,"\u5DF2\u7528 / \u603B\u91CF",v,20,142,250);
    kpi(mem,"\u7F13\u5B58",cache,310,142,150);
    char sw_tot[24],sw_used[24];
    nas_format_bytes(s_status.memory.swap_total_bytes,sw_tot,sizeof(sw_tot));
    nas_format_bytes(s_status.memory.swap_used_bytes,sw_used,sizeof(sw_used));
    fmt_pct(s_status.memory.swap_used_pct,v,sizeof(v)); kpi(mem,"Swap \u5360\u7528",v,20,190,130);
    bar(mem,20,240,420,pct_i(s_status.memory.swap_used_pct),C_AMBER);

    lv_obj_t *health=card(s_content,24,304,976,140,hc(s_status.cpu.health),"\u5065\u5EB7\u72B6\u6001");
    chip(health,ht(s_status.cpu.health),hc(s_status.cpu.health),24,56,140);
    chip(health,ht(s_status.memory.health),hc(s_status.memory.health),194,56,140);
    lbl(health,"CPU / \u5185\u5B58\u72B6\u6001\u7531 NAS Agent \u6C47\u603B\u5224\u65AD\u3002",C_SUBTEXT,24,106,900);
}

/* ── STORAGE ──────────────────────────────────────── */

static void draw_storage(void)
{
    char v[128];
    lv_obj_t *pc=card(s_content,24,18,976,228,C_AMBER,"\u5B58\u50A8\u6C60 / RAID");
    if (!s_status.storage.pool_count) lbl(pc,"\u6682\u65E0\u5B58\u50A8\u6C60\u6570\u636E",C_SUBTEXT,24,72,400);
    for (int i=0;i<s_status.storage.pool_count&&i<3;i++) {
        const nas_pool_t *p=&s_status.storage.pools[i];
        int x=24+i*310;
        char ts[24],fs[24];
        nas_format_bytes(p->total_bytes,ts,sizeof(ts)); nas_format_bytes(p->free_bytes,fs,sizeof(fs));
        lbl(pc,s(p->name,p->id),C_TEXT,x,54,270);
        snprintf(v,sizeof(v),"%s \u00B7 %s",s(p->raid_type,"--"),ht(p->raid_status));
        lbl(pc,v,C_SUBTEXT,x,86,270);
        fmt_pct(p->used_pct,v,sizeof(v)); kpi(pc,"\u5DF2\u7528",v,x,118,90);
        bar(pc,x+86,130,178,pct_i(p->used_pct),C_AMBER);
        snprintf(v,sizeof(v),"%s \u5269\u4F59 %s",ts,fs);
        lbl(pc,v,C_TEXT,x,158,270);
    }

    lv_obj_t *vc=card(s_content,24,280,976,164,C_BLUE,"\u5377\u5BB9\u91CF");
    if (!s_status.storage.volume_count) lbl(vc,"\u6682\u65E0\u5377\u6570\u636E",C_SUBTEXT,24,72,400);
    for (int i=0;i<s_status.storage.volume_count&&i<4;i++) {
        const nas_volume_t *vol=&s_status.storage.volumes[i];
        int cx=24+(i%2)*470,cy=54+(i/2)*76;
        char us[24],fs[24];
        nas_format_bytes(vol->used_bytes,us,sizeof(us)); nas_format_bytes(vol->free_bytes,fs,sizeof(fs));
        snprintf(v,sizeof(v),"%s \u00B7 %s",s(vol->name,vol->id),s(vol->filesystem,"--"));
        lbl(vc,v,C_TEXT,cx,cy,280);
        fmt_pct(vol->used_pct,v,sizeof(v)); lbl(vc,v,C_TEXT,cx+300,cy,60);
        bar(vc,cx,cy+26,360,pct_i(vol->used_pct),C_BLUE);
        snprintf(v,sizeof(v),"\u5DF2\u7528 %s / \u5269\u4F59 %s",us,fs);
        lbl(vc,v,C_SUBTEXT,cx,cy+46,360);
    }
}

/* ── DRIVES ───────────────────────────────────────── */

static void draw_drives(void)
{
    if (!s_status.drive_count) { draw_empty("\u6682\u65E0 HDD / SSD \u6570\u636E"); return; }
    char v[96];
    for (int i=0;i<s_status.drive_count&&i<8;i++) {
        const nas_drive_t *d=&s_status.drives[i];
        int col=i%4,row=i/4,x=24+col*248,y=18+row*226;
        lv_obj_t *c=card(s_content,x,y,224,200,hc(d->health),s(d->bay,d->id));
        snprintf(v,sizeof(v),"%s \u00B7 %s",dt(d->type),ht(d->smart_status));
        lbl(c,v,C_SUBTEXT,16,50,180);
        char cap[24]; nas_format_bytes(d->capacity_bytes,cap,sizeof(cap));
        kpi_temp(c,"\u6E29\u5EA6",d->temperature_c,16,80,90);
        kpi(c,"\u5BB9\u91CF",cap,122,80,86);
        snprintf(v,sizeof(v),"%d",d->bad_sector_count); kpi(c,"\u574F\u5757",v,16,136,80);
        snprintf(v,sizeof(v),"%d \u5C0F\u65F6",d->power_on_hours); kpi(c,"\u901A\u7535",v,122,136,86);
    }
}

/* ── NVME ─────────────────────────────────────────── */

static void draw_nvme(void)
{
    if (!s_status.nvme_count) { draw_empty("\u6682\u65E0 M.2 / NVMe \u6570\u636E"); return; }
    char v[96];
    for (int i=0;i<s_status.nvme_count&&i<6;i++) {
        const nas_nvme_t *n=&s_status.nvme[i];
        int col=i%3,row=i/3,x=24+col*330,y=18+row*226;
        lv_obj_t *c=card(s_content,x,y,306,200,hc(n->health),s(n->slot,n->id));
        lbl(c,s(n->model,"NVMe \u56FA\u6001"),C_TEXT,16,48,260);
        char cap[24],us[24];
        nas_format_bytes(n->capacity_bytes,cap,sizeof(cap)); nas_format_bytes(n->used_bytes,us,sizeof(us));
        kpi(c,"\u5BB9\u91CF",cap,16,82,100); kpi(c,"\u5DF2\u7528",us,156,82,100);
        kpi_temp(c,"\u6E29\u5EA6",n->temperature_c,16,138,80);
        int wear=n->percentage_used_pct>0?n->percentage_used_pct:n->wear_pct;
        snprintf(v,sizeof(v),"%d%% \u78E8\u635F",wear); kpi(c,"\u5BFF\u547D",v,122,138,80);
        chip(c,ht(n->cache_state),C_PURPLE,206,134,76);
    }
}

/* ── NETWORK ──────────────────────────────────────── */

static void draw_network(void)
{
    char v[96],rx[24],tx[24];

    lv_obj_t *sm=card(s_content,24,18,976,130,C_TEAL,"\u7F51\u7EDC\u603B\u89C8");
    nas_format_bps(s_status.network.total_rx_bps,rx,sizeof(rx));
    nas_format_bps(s_status.network.total_tx_bps,tx,sizeof(tx));
    kpi(sm,"\u4E0B\u8F7D",rx,28,54,180); kpi(sm,"\u4E0A\u4F20",tx,234,54,180);
    snprintf(v,sizeof(v),"%d \u4E2A\u7F51\u53E3",s_status.network.interface_count);
    kpi(sm,"\u7F51\u53E3",v,440,54,120);
    chip(sm,ht(s_status.network.health),hc(s_status.network.health),616,56,110);

    for (int i=0;i<s_status.network.interface_count&&i<6;i++) {
        const nas_interface_t *iface=&s_status.network.interfaces[i];
        int col=i%3,row=i/3,x=24+col*330,y=176+row*144;
        lv_color_t stc=sc(iface->status);
        lv_obj_t *c=card(s_content,x,y,306,128,stc,s(iface->name,"\u7F51\u53E3"));
        dot(c,iface->status,16,46);
        snprintf(v,sizeof(v),"%s  \u00B7  %d Mbps",ht(iface->status),iface->link_speed_mbps);
        lbl(c,v,C_SUBTEXT,30,44,250);
        lbl(c,s(iface->ip,"--"),C_TEXT,16,72,250);
        snprintf(v,sizeof(v),"\u9519\u8BEF %d/%d  \u00B7  \u4E22\u5305 %d/%d",iface->rx_errors,iface->tx_errors,iface->rx_dropped,iface->tx_dropped);
        lbl(c,v,C_SUBTEXT,16,100,270);
    }
}

/* ── APPS ─────────────────────────────────────────── */

static void draw_apps(void)
{
    char v[96];
    lv_obj_t *sm=card(s_content,24,18,976,130,C_ORANGE,"Docker / \u4EFB\u52A1");
    snprintf(v,sizeof(v),"%d \u4E2A\u8FD0\u884C",s_status.workloads.docker.running); kpi(sm,"\u8FD0\u884C",v,28,54,140);
    snprintf(v,sizeof(v),"%d \u4E2A\u505C\u6B62",s_status.workloads.docker.stopped); kpi(sm,"\u505C\u6B62",v,210,54,140);
    snprintf(v,sizeof(v),"%d \u4E2A\u5F02\u5E38",s_status.workloads.docker.unhealthy); kpi(sm,"\u5F02\u5E38",v,392,54,140);
    snprintf(v,sizeof(v),"\u5907\u4EFD %d \u00B7 \u5FEB\u7167 %d",s_status.workloads.backup_count,s_status.workloads.snapshot_count);
    kpi(sm,"\u5907\u4EFD/\u5FEB\u7167",v,574,54,260);

    lv_obj_t *list=card(s_content,24,182,976,262,C_BLUE,"\u670D\u52A1\u5217\u8868");
    if (!s_status.workloads.docker.container_count) lbl(list,"\u6682\u65E0\u5BB9\u5668\u6570\u636E",C_SUBTEXT,24,74,300);
    for (int i=0;i<s_status.workloads.docker.container_count&&i<NAS_MAX_CONTAINERS&&i<10;i++) {
        const nas_container_t *ct=&s_status.workloads.docker.containers[i];
        int col=i%2,row=i/2,x=24+col*460,y=54+row*42;
        lbl(list,s(ct->name,"\u5BB9\u5668"),C_TEXT,x,y,260);
        snprintf(v,sizeof(v),"%s / %s",ht(ct->state),ht(ct->health));
        chip(list,v,hc(ct->health),x+280,y-2,130);
    }
}

/* ── SETTINGS ─────────────────────────────────────── */

static bool valid_host(const char *h) {
    if (!h||!*h||strlen(h)>=sizeof(s_host)) return false;
    for (const char *p=h;*p;p++) { unsigned char ch=*p; if (isspace(ch)||ch=='/'||ch==':'||ch=='?'||ch=='#') return false; }
    return true;
}
static int parse_p(const char *t) {
    if (!t||!*t) return -1;
    char *e=NULL; long p=strtol(t,&e,10);
    if (e==t||*e||p<=0||p>65535) return -1;
    return (int)p;
}
static void set_hint(const char *t, lv_color_t c) { if (s_settings_hint) { lv_label_set_text(s_settings_hint,t); lv_obj_set_style_text_color(s_settings_hint,c,0); } }

static void kb_evt(lv_event_t *e) {
    if (lv_event_get_code(e)==LV_EVENT_READY||lv_event_get_code(e)==LV_EVENT_CANCEL)
        lv_obj_add_flag(lv_event_get_target(e),LV_OBJ_FLAG_HIDDEN);
}
static void show_kb(lv_obj_t *ta) {
    if (!s_keyboard||!ta) return;
    lv_keyboard_set_textarea(s_keyboard,ta);
    lv_keyboard_set_mode(s_keyboard,ta==s_port_ta?LV_KEYBOARD_MODE_NUMBER:LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_state(ta,LV_STATE_FOCUSED); lv_obj_clear_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(s_keyboard);
}
static void ta_evt(lv_event_t *e) {
    lv_event_code_t c=lv_event_get_code(e);
    if (c==LV_EVENT_PRESSED||c==LV_EVENT_FOCUSED||c==LV_EVENT_CLICKED) show_kb(lv_event_get_target(e));
}
static void hs_evt(lv_event_t *e) {
    lv_event_code_t c=lv_event_get_code(e);
    if (c!=LV_EVENT_PRESSED&&c!=LV_EVENT_CLICKED&&c!=LV_EVENT_RELEASED) return;
    show_kb((lv_obj_t*)lv_event_get_user_data(e));
}
static void add_hs(lv_obj_t *p,lv_obj_t *ta,int x,int y,int w,int h) {
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_ext_click_area(o,8); lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o,hs_evt,LV_EVENT_ALL,ta);
}
static void ensure_kb(void) {
    if (s_keyboard&&lv_obj_is_valid(s_keyboard)) return;
    s_keyboard=lv_keyboard_create(lv_layer_top()); lv_obj_set_size(s_keyboard,900,220);
    lv_obj_align(s_keyboard,LV_ALIGN_BOTTOM_MID,0,-6);
    lv_obj_set_style_text_font(s_keyboard,&lv_font_montserrat_20,0);
    lv_obj_set_style_bg_color(s_keyboard,C_CARD,0); lv_obj_set_style_bg_opa(s_keyboard,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(s_keyboard,2,0); lv_obj_set_style_border_color(s_keyboard,C_BLUE,0);
    lv_obj_add_event_cb(s_keyboard,kb_evt,LV_EVENT_ALL,NULL); lv_obj_add_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN);
}
static void save_evt(lv_event_t *e) {
    if (lv_event_get_code(e)!=LV_EVENT_CLICKED||!s_host_ta||!s_port_ta) return;
    const char *h=lv_textarea_get_text(s_host_ta); int p=parse_p(lv_textarea_get_text(s_port_ta));
    if (!valid_host(h)||p<0) { set_hint("\u5730\u5740\u6216\u7AEF\u53E3\u683C\u5F0F\u4E0D\u6B63\u786E",C_RED); return; }
    if (s_save_cb&&!s_save_cb(h,p,s_save_user_data)) { set_hint("\u4FDD\u5B58\u5931\u8D25",C_RED); return; }
    strlcpy(s_host,h,sizeof(s_host)); s_port=p;
    strlcpy(s_msg,"\u5730\u5740\u5DF2\u4FDD\u5B58",sizeof(s_msg)); set_hint("\u5DF2\u4FDD\u5B58\uFF0C\u4E0B\u6B21\u5237\u65B0\u751F\u6548",C_GREEN);
    if (s_keyboard) lv_obj_add_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(s_host_ta,LV_STATE_FOCUSED); lv_obj_clear_state(s_port_ta,LV_STATE_FOCUSED);
    lv_obj_invalidate(s_header);
}
static void draw_settings(void) {
    char v[24]; ensure_kb();
    lv_obj_t *pn=card(s_content,104,34,816,280,C_GRAY,"NAS Agent \u5730\u5740");
    lbl(pn,"\u4E3B\u673A\u5730\u5740",C_SUBTEXT,30,58,120);
    s_host_ta=lv_textarea_create(pn); lv_obj_add_style(s_host_ta,&sty_ta,0);
    lv_obj_set_pos(s_host_ta,148,44); lv_obj_set_size(s_host_ta,430,50);
    lv_textarea_set_one_line(s_host_ta,true); lv_textarea_set_max_length(s_host_ta,sizeof(s_host)-1);
    lv_textarea_set_accepted_chars(s_host_ta,"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_");
    lv_textarea_set_placeholder_text(s_host_ta,"IP \u6216\u57DF\u540D"); lv_textarea_set_text(s_host_ta,s_host);
    lv_obj_add_flag(s_host_ta,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_host_ta,ta_evt,LV_EVENT_ALL,NULL);

    lbl(pn,"\u7AEF\u53E3",C_SUBTEXT,30,128,120);
    s_port_ta=lv_textarea_create(pn); lv_obj_add_style(s_port_ta,&sty_ta,0);
    lv_obj_set_pos(s_port_ta,148,114); lv_obj_set_size(s_port_ta,160,50);
    lv_textarea_set_one_line(s_port_ta,true); lv_textarea_set_max_length(s_port_ta,5);
    lv_textarea_set_accepted_chars(s_port_ta,"0123456789");
    snprintf(v,sizeof(v),"%d",s_port); lv_textarea_set_text(s_port_ta,v);
    lv_obj_add_flag(s_port_ta,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_port_ta,ta_evt,LV_EVENT_ALL,NULL);

    lv_obj_t *sv=lv_btn_create(pn); lv_obj_add_style(sv,&sty_btn,0);
    lv_obj_set_style_bg_color(sv,C_GREEN,0); lv_obj_set_pos(sv,616,72); lv_obj_set_size(sv,150,70);
    lv_obj_add_event_cb(sv,save_evt,LV_EVENT_CLICKED,NULL);
    lv_obj_t *st=lbl(sv,"\u4FDD\u5B58",C_WHITE,0,23,150); lv_obj_set_style_text_align(st,LV_TEXT_ALIGN_CENTER,0);

    s_settings_hint=lbl(pn,"\u652F\u6301\u57DF\u540D\u6216 IPv4 \u5730\u5740\uFF0C\u7AEF\u53E3\u8303\u56F4 1-65535",C_MUTED,30,210,720);

    lv_obj_t *cur=card(s_content,104,346,816,74,C_GREEN,"\u5F53\u524D\u8FDE\u63A5");
    char ep[128]; snprintf(ep,sizeof(ep),"%s:%d",s_host,s_port);
    lbl(cur,ep,C_TEXT,24,38,520);
    chip(cur,s_online?"\u5728\u7EBF":"\u79BB\u7EBF",s_online?C_GREEN:C_RED,638,28,110);

    add_hs(pn,s_host_ta,148,44,430,50); add_hs(pn,s_port_ta,148,114,160,50);
    lv_keyboard_set_textarea(s_keyboard,s_host_ta);
    lv_obj_add_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(s_keyboard);
}

/* ── navigation ───────────────────────────────────── */

static void set_page(ui_page_t pg)
{
    if (pg>=PAGE_COUNT||pg==s_page) return;
    s_page=pg;
    for (int i=0;i<PAGE_COUNT;i++) {
        if (s_nav_dots[i])
            lv_obj_set_style_bg_opa(s_nav_dots[i],i==pg?LV_OPA_COVER:LV_OPA_TRANSP,0);
    }
    lv_label_set_text(s_page_label,PAGE_NAMES[pg]);
    lv_obj_set_style_bg_color(s_content,PAGE_LIGHT[pg],0);
    lv_obj_set_style_bg_color(s_header,PAGE_LIGHT[pg],0);
    clear_content();
    if (!s_has_status&&pg!=PAGE_SETTINGS) {
        lv_obj_t *ec=card(s_content,212,140,600,200,PAGE_COLORS[pg],"\u7B49\u5F85\u6570\u636E");
        lbl(ec,"\u8FD8\u6CA1\u6709\u6536\u5230 NAS \u72B6\u6001\u6570\u636E",C_TEXT,24,60,552);
        lbl(ec,"\u8BF7\u786E\u8BA4 NAS \u7AEF Docker Agent \u5DF2\u542F\u52A8\uFF0C\u5E76\u4E14\u8BBE\u5907\u80FD\u8BBF\u95EE\u8BE5\u5730\u5740\u3002",C_SUBTEXT,24,92,552);
        return;
    }
    switch (pg) {
    case PAGE_HOME: draw_home(); break;
    case PAGE_PERFORMANCE: draw_performance(); break;
    case PAGE_STORAGE: draw_storage(); break;
    case PAGE_DRIVES: draw_drives(); break;
    case PAGE_NVME: draw_nvme(); break;
    case PAGE_NETWORK: draw_network(); break;
    case PAGE_APPS: draw_apps(); break;
    case PAGE_SETTINGS: draw_settings(); break;
    default: break;
    }
}

static void nav_evt(lv_event_t *e)
{
    if (s_keyboard&&!lv_obj_has_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN)) return;
    set_page((ui_page_t)(intptr_t)lv_event_get_user_data(e));
}

static void gesture_evt(lv_event_t *e)
{
    (void)e;
    if (s_keyboard&&!lv_obj_has_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN)) return;
    lv_indev_t *indev=lv_indev_get_act(); if (!indev) return;
    lv_dir_t dir=lv_indev_get_gesture_dir(indev);
    if (dir==LV_DIR_LEFT)  set_page((ui_page_t)((s_page+1)%PAGE_COUNT));
    if (dir==LV_DIR_RIGHT) set_page((ui_page_t)((s_page+PAGE_COUNT-1)%PAGE_COUNT));
}

static void build_nav(void)
{
    s_nav=lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_nav);
    lv_obj_set_style_bg_color(s_nav,C_NAV_WHITE,0);
    lv_obj_set_style_bg_opa(s_nav,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(s_nav,0,0);
    lv_obj_set_style_shadow_width(s_nav,4,0);
    lv_obj_set_style_shadow_color(s_nav,C_SHADOW,0);
    lv_obj_set_style_shadow_ofs_y(s_nav,-2,0);
    lv_obj_set_style_pad_all(s_nav,0,0);
    lv_obj_set_pos(s_nav,0,SCREEN_H-NAV_H);
    lv_obj_set_size(s_nav,SCREEN_W,NAV_H);
    lv_obj_add_flag(s_nav,LV_OBJ_FLAG_GESTURE_BUBBLE);

    int btn_w=SCREEN_W/PAGE_COUNT;
    for (int i=0;i<PAGE_COUNT;i++) {
        lv_obj_t *btn=lv_btn_create(s_nav);
        lv_obj_remove_style_all(btn);
        lv_obj_set_style_bg_opa(btn,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(btn,0,0);
        lv_obj_set_style_shadow_width(btn,0,0);
        lv_obj_set_pos(btn,i*btn_w,0);
        lv_obj_set_size(btn,btn_w,NAV_H);
        lv_obj_add_event_cb(btn,nav_evt,LV_EVENT_CLICKED,(void*)(intptr_t)i);
        s_nav_btns[i]=btn;

        lv_obj_t *dot=lv_obj_create(btn);
        lv_obj_remove_style_all(dot);
        lv_obj_set_style_bg_color(dot,PAGE_COLORS[i],0);
        lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(dot,i==0?LV_OPA_COVER:LV_OPA_TRANSP,0);
        lv_obj_set_pos(dot,(btn_w-8)/2,4);
        lv_obj_set_size(dot,8,8);
        s_nav_dots[i]=dot;

        lv_obj_t *txt=lbl(btn,PAGE_NAMES[i],i==0?PAGE_COLORS[i]:C_SUBTEXT,0,18,btn_w);
        lv_obj_set_style_text_align(txt,LV_TEXT_ALIGN_CENTER,0);
    }
}

static void refresh_nav(void)
{
    for (int i=0;i<PAGE_COUNT;i++) {
        if (s_nav_dots[i])
            lv_obj_set_style_bg_opa(s_nav_dots[i],i==s_page?LV_OPA_COVER:LV_OPA_TRANSP,0);
        if (s_nav_btns[i]) {
            lv_obj_t *txt=lv_obj_get_child(s_nav_btns[i],1);
            if (txt) lv_obj_set_style_text_color(txt,i==s_page?PAGE_COLORS[i]:C_SUBTEXT,0);
        }
    }
}

/* ── init / lifecycle ─────────────────────────────── */

static void init_styles(void)
{
    lv_style_init(&sty_screen);
    lv_style_set_bg_color(&sty_screen,C_BG); lv_style_set_bg_opa(&sty_screen,LV_OPA_COVER);

    lv_style_init(&sty_card);
    lv_style_set_bg_color(&sty_card,C_CARD); lv_style_set_bg_opa(&sty_card,LV_OPA_COVER);
    lv_style_set_radius(&sty_card,RADIUS_CARD);
    lv_style_set_border_width(&sty_card,1); lv_style_set_border_color(&sty_card,C_CARD_BORDER);
    lv_style_set_shadow_width(&sty_card,8); lv_style_set_shadow_ofs_x(&sty_card,2);
    lv_style_set_shadow_ofs_y(&sty_card,4); lv_style_set_shadow_color(&sty_card,C_SHADOW);
    lv_style_set_pad_all(&sty_card,0);

    lv_style_init(&sty_chip);
    lv_style_set_bg_color(&sty_chip,C_CHIP_BG); lv_style_set_bg_opa(&sty_chip,LV_OPA_COVER);
    lv_style_set_radius(&sty_chip,8); lv_style_set_border_width(&sty_chip,1); lv_style_set_pad_all(&sty_chip,0);

    lv_style_init(&sty_bar_bg);
    lv_style_set_bg_color(&sty_bar_bg,C_BAR_BG); lv_style_set_bg_opa(&sty_bar_bg,LV_OPA_COVER);
    lv_style_set_radius(&sty_bar_bg,6);
    lv_style_init(&sty_bar_ind); lv_style_set_radius(&sty_bar_ind,6);

    lv_style_init(&sty_ta);
    lv_style_set_bg_color(&sty_ta,C_TA_BG); lv_style_set_bg_opa(&sty_ta,LV_OPA_COVER);
    lv_style_set_radius(&sty_ta,10); lv_style_set_border_width(&sty_ta,2);
    lv_style_set_border_color(&sty_ta,C_TA_BORDER);
    lv_style_set_pad_left(&sty_ta,12); lv_style_set_pad_top(&sty_ta,8);
    lv_style_set_text_font(&sty_ta,FONT_CN); lv_style_set_text_color(&sty_ta,C_TEXT);

    lv_style_init(&sty_btn); lv_style_set_radius(&sty_btn,10);
    lv_style_set_border_width(&sty_btn,0); lv_style_set_shadow_width(&sty_btn,0);
}

void ui_set_endpoint_config(const char *host, int port)
{
    if (host&&*host) strlcpy(s_host,host,sizeof(s_host));
    if (port>0&&port<=65535) s_port=port;
}
void ui_set_endpoint_save_callback(ui_endpoint_save_cb_t cb, void *ud) { s_save_cb=cb; s_save_user_data=ud; }

void ui_init(void)
{
    if (!board_lvgl_lock(500)) return;
    init_styles(); nas_status_init(&s_status);

    s_scr=lv_scr_act(); lv_obj_remove_style_all(s_scr);
    lv_obj_add_style(s_scr,&sty_screen,0);
    lv_obj_clear_flag(s_scr,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_scr,gesture_evt,LV_EVENT_GESTURE,NULL);

    s_header=lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_header);
    lv_obj_set_style_bg_color(s_header,PAGE_LIGHT[0],0);
    lv_obj_set_style_bg_opa(s_header,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(s_header,0,0);
    lv_obj_set_style_pad_all(s_header,0,0);
    lv_obj_set_pos(s_header,0,0); lv_obj_set_size(s_header,SCREEN_W,HEADER_H);
    lv_obj_add_flag(s_header,LV_OBJ_FLAG_GESTURE_BUBBLE);

    s_page_label=lbl(s_header,PAGE_NAMES[0],PAGE_COLORS[0],20,12,120);
    s_endpoint_label=lbl(s_header,"",C_SUBTEXT,150,12,560);
    lv_obj_set_style_text_font(s_endpoint_label,FONT_SMALL,0);

    s_status_chip=lbl(s_header,"\u79BB\u7EBF",C_WHITE,0,0,72);
    lv_obj_add_style(s_status_chip,&sty_chip,0);
    lv_obj_set_style_bg_color(s_status_chip,C_RED,0);
    lv_obj_set_style_border_width(s_status_chip,0,0);
    lv_obj_set_pos(s_status_chip,820,12); lv_obj_set_size(s_status_chip,72,32);
    lv_obj_set_style_text_align(s_status_chip,LV_TEXT_ALIGN_CENTER,0);

    s_content=lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_pos(s_content,0,CONTENT_Y); lv_obj_set_size(s_content,SCREEN_W,CONTENT_H);
    lv_obj_set_style_bg_color(s_content,PAGE_LIGHT[0],0); lv_obj_set_style_bg_opa(s_content,LV_OPA_COVER,0);
    lv_obj_clear_flag(s_content,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_content,LV_OBJ_FLAG_GESTURE_BUBBLE);

    build_nav();

    char ep[200]; snprintf(ep,sizeof(ep),"%s:%d  %s",s_host,s_port,s_msg);
    lv_label_set_text(s_endpoint_label,ep);
    draw_home();
    board_lvgl_unlock();
}

void ui_set_message(const char *m)
{
    if (!board_lvgl_lock(500)) return;
    strlcpy(s_msg,s(m,""),sizeof(s_msg));
    char ep[200]; snprintf(ep,sizeof(ep),"%s:%d  %s",s_host,s_port,s_msg);
    lv_label_set_text(s_endpoint_label,ep);
    if ((!s_has_status||s_page==PAGE_SETTINGS)&&
        (s_page!=PAGE_SETTINGS||!s_keyboard||lv_obj_has_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN)))
        set_page(s_page);
    board_lvgl_unlock();
}

void ui_update_status(const nas_status_t *status, bool online)
{
    if (!status||!board_lvgl_lock(500)) return;
    s_status=*status; s_has_status=true; s_online=online;
    strlcpy(s_msg,online?"\u6570\u636E\u5DF2\u5237\u65B0":"\u8FDE\u63A5\u5931\u8D25",sizeof(s_msg));
    char ep[200]; snprintf(ep,sizeof(ep),"%s:%d  %s",s_host,s_port,s_msg);
    lv_label_set_text(s_endpoint_label,ep);
    lv_label_set_text(s_status_chip,online?"\u5728\u7EBF":"\u79BB\u7EBF");
    lv_obj_set_style_bg_color(s_status_chip,online?C_GREEN:C_RED,0);
    refresh_nav();
    if (s_page!=PAGE_SETTINGS||!s_keyboard||lv_obj_has_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN))
        set_page(s_page);
    board_lvgl_unlock();
}
