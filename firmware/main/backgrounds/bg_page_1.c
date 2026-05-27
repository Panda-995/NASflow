#include "page_backgrounds.h"

extern const uint8_t bg_page_1_bin_start[] asm("_binary_bg_page_1_bin_start");

const lv_img_dsc_t bg_page_1 = {
    .header.always_zero = 0,
    .header.w = 1024,
    .header.h = 534,
    .data_size = 1024 * 534 * LV_COLOR_SIZE / 8,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = bg_page_1_bin_start,
};
