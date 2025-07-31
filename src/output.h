#ifndef WD_OUTPUT_H
#define WD_OUTPUT_H

void wd_list_outputs();
void wd_init_output();
struct SDL_Window* wd_get_output_window();
void wd_free_output();

#endif
