#ifndef WD_HYPRLAND_H
#define WD_HYPRLAND_H

#include <stdbool.h>

typedef struct {
    int socket;
    bool output_hidden;
    bool output_hidden_valid;
} wd_hyprland_state;

void wd_hyprland_init(wd_hyprland_state* state);
bool wd_hyprland_output_hidden(wd_hyprland_state* state, const char* output_name);
void wd_hyprland_free(wd_hyprland_state* state);

#endif
