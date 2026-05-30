#include "power_monitor.h"

#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "power_monitor";

#ifndef CONFIG_NAS_DISPLAY_BATTERY_ADC_GPIO
#define CONFIG_NAS_DISPLAY_BATTERY_ADC_GPIO -1
#endif
#ifndef CONFIG_NAS_DISPLAY_BATTERY_ADC_ATTEN_DB
#define CONFIG_NAS_DISPLAY_BATTERY_ADC_ATTEN_DB 12
#endif
#ifndef CONFIG_NAS_DISPLAY_BATTERY_ADC_RATIO_MILLI
#define CONFIG_NAS_DISPLAY_BATTERY_ADC_RATIO_MILLI 2000
#endif
#ifndef CONFIG_NAS_DISPLAY_CHARGE_GPIO
#define CONFIG_NAS_DISPLAY_CHARGE_GPIO -1
#endif
#ifndef CONFIG_NAS_DISPLAY_CHARGE_ACTIVE_LOW
#define CONFIG_NAS_DISPLAY_CHARGE_ACTIVE_LOW 1
#endif
#ifndef CONFIG_NAS_DISPLAY_DONE_GPIO
#define CONFIG_NAS_DISPLAY_DONE_GPIO -1
#endif
#ifndef CONFIG_NAS_DISPLAY_DONE_ACTIVE_LOW
#define CONFIG_NAS_DISPLAY_DONE_ACTIVE_LOW 1
#endif
#ifndef CONFIG_NAS_DISPLAY_USB_DETECT_GPIO
#define CONFIG_NAS_DISPLAY_USB_DETECT_GPIO -1
#endif
#ifndef CONFIG_NAS_DISPLAY_USB_DETECT_ACTIVE_HIGH
#define CONFIG_NAS_DISPLAY_USB_DETECT_ACTIVE_HIGH 1
#endif

static adc_oneshot_unit_handle_t s_adc_unit;
static adc_channel_t s_adc_channel;
static adc_cali_handle_t s_cali;
static bool s_adc_ready;
static bool s_cali_ready;

static adc_atten_t configured_atten(void)
{
    switch (CONFIG_NAS_DISPLAY_BATTERY_ADC_ATTEN_DB) {
    case 0:
        return ADC_ATTEN_DB_0;
    case 2:
        return ADC_ATTEN_DB_2_5;
    case 6:
        return ADC_ATTEN_DB_6;
    case 12:
    default:
        return ADC_ATTEN_DB_12;
    }
}

static bool gpio_configured(int gpio)
{
    return gpio >= 0 && gpio < GPIO_NUM_MAX;
}

static void configure_status_gpio(int gpio, bool pull_up)
{
    if (!gpio_configured(gpio)) {
        return;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

static bool active_level(int gpio, bool active_high)
{
    if (!gpio_configured(gpio)) {
        return false;
    }
    int level = gpio_get_level((gpio_num_t)gpio);
    return active_high ? level == 1 : level == 0;
}

static int voltage_to_pct(int mv)
{
    static const struct {
        int mv;
        int pct;
    } table[] = {
        {4200, 100},
        {4100, 90},
        {4000, 80},
        {3920, 70},
        {3850, 60},
        {3790, 50},
        {3730, 40},
        {3680, 30},
        {3600, 20},
        {3500, 10},
        {3300, 0},
    };

    if (mv >= table[0].mv) {
        return 100;
    }
    int last = (int)(sizeof(table) / sizeof(table[0])) - 1;
    if (mv <= table[last].mv) {
        return 0;
    }

    for (int i = 0; i < last; ++i) {
        if (mv <= table[i].mv && mv >= table[i + 1].mv) {
            int range_mv = table[i].mv - table[i + 1].mv;
            int range_pct = table[i].pct - table[i + 1].pct;
            int offset_mv = mv - table[i + 1].mv;
            return table[i + 1].pct + (offset_mv * range_pct) / range_mv;
        }
    }
    return -1;
}

static bool read_battery_voltage(int *voltage_mv)
{
    if (!s_adc_ready || voltage_mv == NULL) {
        return false;
    }

    int total_mv = 0;
    int ok_count = 0;
    for (int i = 0; i < 8; ++i) {
        int adc_mv = 0;
        esp_err_t err = ESP_FAIL;
        if (s_cali_ready) {
            err = adc_oneshot_get_calibrated_result(s_adc_unit, s_cali, s_adc_channel, &adc_mv);
        } else {
            int raw = 0;
            err = adc_oneshot_read(s_adc_unit, s_adc_channel, &raw);
            adc_mv = (raw * 3300) / 4095;
        }
        if (err == ESP_OK) {
            total_mv += adc_mv;
            ok_count++;
        }
    }

    if (ok_count == 0) {
        return false;
    }

    int avg_adc_mv = total_mv / ok_count;
    *voltage_mv = (avg_adc_mv * CONFIG_NAS_DISPLAY_BATTERY_ADC_RATIO_MILLI) / 1000;
    return true;
}

esp_err_t power_monitor_init(void)
{
    configure_status_gpio(CONFIG_NAS_DISPLAY_CHARGE_GPIO, CONFIG_NAS_DISPLAY_CHARGE_ACTIVE_LOW);
    configure_status_gpio(CONFIG_NAS_DISPLAY_DONE_GPIO, CONFIG_NAS_DISPLAY_DONE_ACTIVE_LOW);
    configure_status_gpio(CONFIG_NAS_DISPLAY_USB_DETECT_GPIO, false);

    if (!gpio_configured(CONFIG_NAS_DISPLAY_BATTERY_ADC_GPIO)) {
        ESP_LOGW(TAG, "battery ADC is not configured; power telemetry will show unknown");
        return ESP_OK;
    }

    adc_unit_t unit_id;
    adc_channel_t channel;
    esp_err_t err = adc_oneshot_io_to_channel(CONFIG_NAS_DISPLAY_BATTERY_ADC_GPIO, &unit_id, &channel);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "invalid battery ADC GPIO %d: %s", CONFIG_NAS_DISPLAY_BATTERY_ADC_GPIO, esp_err_to_name(err));
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = unit_id,
    };
    err = adc_oneshot_new_unit(&unit_config, &s_adc_unit);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to init battery ADC: %s", esp_err_to_name(err));
        return ESP_OK;
    }

    s_adc_channel = channel;
    adc_atten_t atten = configured_atten();
    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = atten,
    };
    err = adc_oneshot_config_channel(s_adc_unit, channel, &channel_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to config battery ADC channel: %s", esp_err_to_name(err));
        return ESP_OK;
    }

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit_id,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali);
    s_cali_ready = err == ESP_OK;
    if (!s_cali_ready) {
        ESP_LOGW(TAG, "ADC calibration unavailable, using raw voltage fallback: %s", esp_err_to_name(err));
    }

    s_adc_ready = true;
    ESP_LOGI(TAG, "battery ADC configured on GPIO%d", CONFIG_NAS_DISPLAY_BATTERY_ADC_GPIO);
    return ESP_OK;
}

void power_monitor_read(power_status_t *status)
{
    if (status == NULL) {
        return;
    }

    memset(status, 0, sizeof(*status));
    status->voltage_mv = -1;
    status->battery_pct = -1;
    status->source = POWER_SUPPLY_UNKNOWN;
    status->charge_state = POWER_CHARGE_UNKNOWN;

    int voltage_mv = -1;
    if (read_battery_voltage(&voltage_mv)) {
        status->battery_supported = true;
        status->voltage_mv = voltage_mv;
        status->battery_pct = voltage_to_pct(voltage_mv);
    }

    bool charging = active_level(CONFIG_NAS_DISPLAY_CHARGE_GPIO, !CONFIG_NAS_DISPLAY_CHARGE_ACTIVE_LOW);
    bool full = active_level(CONFIG_NAS_DISPLAY_DONE_GPIO, !CONFIG_NAS_DISPLAY_DONE_ACTIVE_LOW);
    status->charge_supported = gpio_configured(CONFIG_NAS_DISPLAY_CHARGE_GPIO) ||
                               gpio_configured(CONFIG_NAS_DISPLAY_DONE_GPIO);
    if (charging) {
        status->charge_state = POWER_CHARGE_CHARGING;
    } else if (full) {
        status->charge_state = POWER_CHARGE_FULL;
    } else if (status->battery_supported) {
        status->charge_state = POWER_CHARGE_DISCHARGING;
    } else if (status->charge_supported) {
        status->charge_state = POWER_CHARGE_NOT_CHARGING;
    }

    bool usb_present = active_level(CONFIG_NAS_DISPLAY_USB_DETECT_GPIO, CONFIG_NAS_DISPLAY_USB_DETECT_ACTIVE_HIGH);
    status->source_supported = gpio_configured(CONFIG_NAS_DISPLAY_USB_DETECT_GPIO) || status->charge_supported;
    if (gpio_configured(CONFIG_NAS_DISPLAY_USB_DETECT_GPIO)) {
        status->source = usb_present ? POWER_SUPPLY_USB : POWER_SUPPLY_BATTERY;
    } else if (charging || full) {
        status->source = POWER_SUPPLY_USB;
    } else if (status->battery_supported) {
        status->source = POWER_SUPPLY_BATTERY;
    }
}
