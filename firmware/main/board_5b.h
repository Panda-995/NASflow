#pragma once

#include "esp_err.h"
#include "lvgl.h"

#define BOARD_LCD_H_RES 1024
#define BOARD_LCD_V_RES 600

esp_err_t board_5b_init(void);
bool board_lvgl_lock(uint32_t timeout_ms);
void board_lvgl_unlock(void);
void board_backlight_set(bool enabled);

