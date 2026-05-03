#ifndef WD_OUTPUT_H
#define WD_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "argparse.h"

typedef struct wd_output_state {
    struct SDL_Window* window;
    void* data;
    struct SDL_Window* (*get_window)(void*);
    bool (*update_output)(void*);
    bool (*output_hidden)(void*);
    void (*deactivate_output)(void*);
    void (*free_output)(void*);
} wd_output_state;

bool wd_init_output(wd_output_state* output, wd_args_state* args, bool opengl);
bool wd_update_output(wd_output_state* output);
bool wd_output_hidden(wd_output_state* output);
void wd_deactivate_output(wd_output_state* output);
void wd_free_output(wd_output_state* output);
bool wd_list_displays(wd_args_state* args);

#endif
