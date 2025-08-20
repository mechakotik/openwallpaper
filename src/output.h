#ifndef WD_OUTPUT_H
#define WD_OUTPUT_H

#include <stdbool.h>
#include "argparse.h"

typedef struct wd_output_state {
    struct SDL_Window* window;
    void* data;
    void (*free_output)(void*);
} wd_output_state;

void wd_list_outputs();
bool wd_init_output(wd_output_state* output, wd_args_state* args);
void wd_free_output(wd_output_state* output);

#endif
