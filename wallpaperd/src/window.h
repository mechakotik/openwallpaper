#ifndef WD_WINDOW_OUTPUT_H
#define WD_WINDOW_OUTPUT_H

#include <stdbool.h>

bool wd_window_output_init(void** data);
struct SDL_Window* wd_window_output_get_window(void* data);
void wd_window_output_free(void* data);

#endif
