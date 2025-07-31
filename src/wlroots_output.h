#ifdef WD_WLROOTS

#ifndef WD_WLROOTS_OUTPUT_H
#define WD_WLROOTS_OUTPUT_H

void wd_wlroots_output_init();
struct SDL_Window* wd_wlroots_output_get_window();
void wd_wlroots_output_free();

#endif

#endif
