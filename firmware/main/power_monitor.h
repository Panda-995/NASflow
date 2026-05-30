#pragma once

#include "esp_err.h"

#include <stdbool.h>

typedef enum {
    POWER_SUPPLY_UNKNOWN = 0,
    POWER_SUPPLY_USB,
    POWER_SUPPLY_BATTERY,
} power_supply_t;

typedef enum {
    POWER_CHARGE_UNKNOWN = 0,
    POWER_CHARGE_CHARGING,
    POWER_CHARGE_FULL,
    POWER_CHARGE_DISCHARGING,
    POWER_CHARGE_NOT_CHARGING,
} power_charge_state_t;

typedef struct {
    bool battery_supported;
    bool charge_supported;
    bool source_supported;
    int voltage_mv;
    int battery_pct;
    power_supply_t source;
    power_charge_state_t charge_state;
} power_status_t;

esp_err_t power_monitor_init(void);
void power_monitor_read(power_status_t *status);
