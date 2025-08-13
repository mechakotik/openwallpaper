#ifndef WD_STATE_H
#define WD_STATE_H

#include <stdint.h>
#include "argparse.h"
#include "object_manager.h"
#include "output.h"

typedef struct wd_state {
    wd_args_state args;
    wd_object_manager_state object_manager;
    wd_output_state output;
} wd_state;

void wd_init_state(wd_state* state);
void wd_free_state(wd_state* state);

#endif
