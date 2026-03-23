#include "output.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "argparse.h"
#include "dynamic_api.h"
#include "error.h"
#include "window.h"
#include "wlroots.h"

static const char* get_output(wd_args_state* args) {
    if(args->window) {
        return "window";
    }
    if(getenv("WAYLAND_DISPLAY") != NULL && wd_dynapi_load_wayland()) {
        return "wlroots";
    }
    return "window";
}

bool wd_init_output(wd_output_state* output, wd_args_state* args) {
    const char* name = get_output(args);

    if(strcmp(name, "window") == 0) {
        if(!wd_window_output_init(&output->data)) {
            return false;
        }
        output->window = wd_window_output_get_window(output->data);
        output->free_output = wd_window_output_free;
    } else if(strcmp(name, "wlroots") == 0) {
#ifdef WD_WLROOTS
        if(!wd_wlroots_output_init(&output->data, args->display)) {
            return false;
        }
        output->window = wd_wlroots_output_get_window(output->data);
        output->output_hidden = wd_wlroots_output_hidden;
        output->free_output = wd_wlroots_output_free;
#else
        wd_set_error("wlroots output support is disabled, compile wallpaperd with -DWD_WLROOTS=ON to use it");
        return false;
#endif
    } else {
        wd_set_error("unknown output '%s'", name);
        return false;
    }

    return true;
}

bool wd_output_hidden(wd_output_state* output) {
    if(output->output_hidden != NULL) {
        return output->output_hidden(output->data);
    }
    return false;
}

void wd_free_output(wd_output_state* output) {
    if(output->free_output != NULL) {
        output->free_output(output->data);
        output->data = NULL;
    }
}

bool wd_list_displays(wd_args_state* args) {
    const char* name = get_output(args);

    if(strcmp(name, "wlroots") == 0) {
#ifdef WD_WLROOTS
        return wd_wlroots_list_displays();
#else
        wd_set_error("wlroots output support is disabled, compile wallpaperd with -DWD_WLROOTS=ON to use it");
        return false;
#endif
    }

    wd_set_error("no available wallpaper output");
    return false;
}
