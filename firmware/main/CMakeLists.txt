idf_component_register(
    SRCS
        "app_main.c"
        "api_client.c"
        "app_config.c"
        "board_5b.c"
        "fonts/lv_font_nas_cn_18.c"
        "nas_status.c"
        "ui.c"
        "wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_event
        esp_http_client
        esp_lcd
        esp_netif
        esp_timer
        esp_wifi
        esp_driver_i2c
        driver
        cjson
        nvs_flash
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
