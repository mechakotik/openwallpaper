#ifdef WD_WLROOTS

#ifndef WD_WLROOTS_H
#define WD_WLROOTS_H

#include <stdbool.h>

bool wd_wlroots_output_init(void** data);
struct SDL_Window* wd_wlroots_output_get_window(void* data);
bool wd_wlroots_output_hidden(void* data);
void wd_wlroots_output_free(void* data);

#endif

#endif
