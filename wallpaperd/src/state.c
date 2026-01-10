#include "state.h"
#include "object_manager.h"
#include "output.h"

void wd_init_state(wd_state* state) {
    *state = (wd_state){0};
    uint32_t unused;
    wd_new_object(&state->object_manager, WD_OBJECT_EMPTY, NULL, &unused);
}

void wd_free_state(wd_state* state) {
    wd_free_audio_visualizer(&state->audio_visualizer);
    wd_free_object_manager(state);
    wd_free_scene(&state->scene);
    wd_free_zip(&state->zip);
    wd_free_output(&state->output);
    wd_free_args(&state->args);
}
