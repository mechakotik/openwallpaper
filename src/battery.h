#ifndef WD_BATTERY_H
#define WD_BATTERY_H

#include <stdbool.h>

typedef enum {
    WD_BATTERY_NONE,
    WD_BATTERY_LINUX_SYSFS,
} wd_battery_interface;

typedef struct {
    wd_battery_interface interface;
} wd_battery_state;

void wd_init_battery(wd_battery_state* battery);
bool wd_battery_discharging(wd_battery_state* battery);

#endif
