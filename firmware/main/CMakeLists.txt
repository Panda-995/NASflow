idf_component_register(
    SRCS
        "app_main.c"
        "api_client.c"
        "app_config.c"
        "board_5b.c"
        "backgrounds/bg_page_1.c"
        "fonts/lv_font_nas_cn_18.c"
        "fonts/lv_font_nas_cn_26.c"
        "nas_status.c"
        "power_monitor.c"
        "ui.c"
        "web_server.c"
        "wifi_manager.c"
    INCLUDE_DIRS "."
    EMBED_FILES bg_page_1.bin
    REQUIRES
        esp_event
        esp_http_client
        esp_http_server
        esp_lcd
        esp_netif
        esp_timer
        esp_wifi
        esp_driver_i2c
        esp_adc
        driver
        cjson
        nvs_flash
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
