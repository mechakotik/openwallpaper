#ifndef WD_STATE_H
#define WD_STATE_H

#include <stdint.h>
#include "argparse.h"
#include "battery.h"
#include "object_manager.h"
#include "output.h"
#include "scene.h"
#include "zip.h"

typedef struct wd_state {
    wd_args_state args;
    wd_object_manager_state object_manager;
    wd_output_state output;
    wd_scene_state scene;
    wd_zip_state zip;
    wd_battery_state battery;
} wd_state;

void wd_init_state(wd_state* state);
void wd_free_state(wd_state* state);

#endif
