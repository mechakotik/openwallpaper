#include "state.h"
#include "object_manager.h"
#include "output.h"

void wd_init_state(wd_state* state) {
    *state = (wd_state){0};
}

void wd_free_state(wd_state* state) {
    wd_free_scene(&state->scene);
    wd_free_output(&state->output);
    wd_free_object_manager(&state->object_manager);
    wd_free_args(&state->args);
}
