#include "battery.h"
#include <stdlib.h>

void wd_init_battery(wd_battery_state* battery) {
    if(system("grep -q . /sys/class/power_supply/*/online") == 0) {
        battery->interface = WD_BATTERY_LINUX_SYSFS;
    } else {
        battery->interface = WD_BATTERY_NONE;
    }
}

bool wd_battery_discharging(wd_battery_state* battery) {
    if(battery->interface == WD_BATTERY_LINUX_SYSFS) {
        return (system("grep -q 0 /sys/class/power_supply/AC*/online") == 0);
    }
    return false;
}
