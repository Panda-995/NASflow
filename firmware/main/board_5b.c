#include "board_5b.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "board_5b";

#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_TIMEOUT_MS 1000
#define TP_IRQ_GPIO GPIO_NUM_4

#define LCD_PIXEL_CLOCK_HZ (21 * 1000 * 1000)
#define LCD_BUFFER_LINES 80

static esp_lcd_panel_handle_t s_lcd;
static esp_lcd_touch_handle_t s_touch;
static SemaphoreHandle_t s_lvgl_mux;
static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_ch422_mode;
static i2c_master_dev_handle_t s_ch422_out;

static esp_err_t add_i2c_device(uint8_t address, i2c_master_dev_handle_t *handle)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    return i2c_master_bus_add_device(s_i2c_bus, &dev_config, handle);
}

static esp_err_t ch422_write(uint8_t address, uint8_t value)
{
    i2c_master_dev_handle_t handle = NULL;
    if (address == 0x24) {
        handle = s_ch422_mode;
    } else if (address == 0x38) {
        handle = s_ch422_out;
    }
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "unknown ch422 address");
    return i2c_master_transmit(handle, &value, 1, I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t i2c_init(void)
{
    if (s_i2c_bus) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = true},
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_i2c_bus), TAG, "i2c bus failed");
    ESP_RETURN_ON_ERROR(add_i2c_device(0x24, &s_ch422_mode), TAG, "ch422 mode device failed");
    ESP_RETURN_ON_ERROR(add_i2c_device(0x38, &s_ch422_out), TAG, "ch422 out device failed");
    return ESP_OK;
}

static void touch_reset(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << TP_IRQ_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ch422_write(0x24, 0x01);
    ch422_write(0x38, 0x2C);
    esp_rom_delay_us(100 * 1000);
    gpio_set_level(TP_IRQ_GPIO, 0);
    esp_rom_delay_us(100 * 1000);
    ch422_write(0x38, 0x2E);
    esp_rom_delay_us(200 * 1000);
}

void board_backlight_set(bool enabled)
{
    ch422_write(0x24, 0x01);
    ch422_write(0x38, enabled ? 0x1E : 0x1A);
}

static esp_err_t lcd_init(void)
{
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = BOARD_LCD_H_RES,
            .v_res = BOARD_LCD_V_RES,
            .hsync_back_porch = 145,
            .hsync_front_porch = 170,
            .hsync_pulse_width = 30,
            .vsync_back_porch = 23,
            .vsync_front_porch = 12,
            .vsync_pulse_width = 2,
            .flags = {.pclk_active_neg = 1},
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .bounce_buffer_size_px = BOARD_LCD_H_RES * 10,
        .dma_burst_size = 64,
        .hsync_gpio_num = GPIO_NUM_46,
        .vsync_gpio_num = GPIO_NUM_3,
        .de_gpio_num = GPIO_NUM_5,
        .pclk_gpio_num = GPIO_NUM_7,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {
            GPIO_NUM_14, GPIO_NUM_38, GPIO_NUM_18, GPIO_NUM_17,
            GPIO_NUM_10, GPIO_NUM_39, GPIO_NUM_0, GPIO_NUM_45,
            GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, GPIO_NUM_1,
            GPIO_NUM_2, GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40,
        },
        .flags = {.fb_in_psram = 1},
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, &s_lcd), TAG, "new rgb panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_lcd), TAG, "lcd panel init failed");
    return ESP_OK;
}

static esp_err_t touch_init(void)
{
    touch_reset();
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_config, &tp_io_handle),
        TAG,
        "touch io failed"
    );

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &s_touch);
}

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(2);
}

static void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    (void)drv;
    esp_lcd_panel_draw_bitmap(s_lcd, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    esp_lcd_touch_point_data_t point[1] = {0};
    uint8_t count = 0;
    esp_lcd_touch_read_data(s_touch);
    esp_err_t err = esp_lcd_touch_get_data(s_touch, point, &count, 1);
    if (err == ESP_OK && count > 0) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = point[0].x;
        data->point.y = point[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        if (board_lvgl_lock(50)) {
            lv_timer_handler();
            board_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static esp_err_t lvgl_init(void)
{
    lv_init();
    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    ESP_RETURN_ON_FALSE(s_lvgl_mux, ESP_ERR_NO_MEM, TAG, "lvgl mutex failed");

    size_t buffer_pixels = BOARD_LCD_H_RES * LCD_BUFFER_LINES;
    lv_color_t *buf1 = heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf1 && buf2, ESP_ERR_NO_MEM, TAG, "lvgl draw buffer failed");

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buffer_pixels);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = BOARD_LCD_H_RES;
    disp_drv.ver_res = BOARD_LCD_V_RES;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    const esp_timer_create_args_t tick_args = {
        .callback = lv_tick_cb,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG, "lv tick timer failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, 2000), TAG, "lv tick start failed");

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL, 1);
    return ESP_OK;
}

bool board_lvgl_lock(uint32_t timeout_ms)
{
    if (!s_lvgl_mux) {
        return false;
    }
    return xSemaphoreTakeRecursive(s_lvgl_mux, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void board_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mux);
}

esp_err_t board_5b_init(void)
{
    ESP_LOGI(TAG, "init i2c");
    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "i2c init failed");
    ESP_LOGI(TAG, "init lcd");
    ESP_RETURN_ON_ERROR(lcd_init(), TAG, "lcd init failed");
    ESP_LOGI(TAG, "init touch");
    ESP_RETURN_ON_ERROR(touch_init(), TAG, "touch init failed");
    ESP_LOGI(TAG, "init lvgl");
    ESP_RETURN_ON_ERROR(lvgl_init(), TAG, "lvgl init failed");
    board_backlight_set(true);
    return ESP_OK;
}
