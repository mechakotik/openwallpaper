#include "output.h"
#include <stdio.h>
#include <string.h>
#include "argparse.h"
#include "error.h"
#include "window_output.h"
#include "wlroots_output.h"

void wd_list_outputs() {
    printf("window");
#ifdef WD_WLROOTS
    printf(" wlroots");
#endif
    printf("\n");
}

static const char* get_default_output() {
    return "window";
}

static const char* get_output(wd_args_state* args) {
    const char* output;
    if(wd_get_option(args, "output") != NULL) {
        output = wd_get_option(args, "output");
    } else {
        output = get_default_output();
    }
    return output;
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
        if(!wd_wlroots_output_init(&output->data)) {
            return false;
        }
        output->window = wd_wlroots_output_get_window(output->data);
        output->free_output = wd_wlroots_output_free;
#else
        wd_set_error("wlroots output support is disabled, compile with -DWD_WLROOTS=ON to use it");
        return false;
#endif
    } else {
        wd_set_error("unknown output '%s'", name);
        return false;
    }

    return true;
}

void wd_free_output(wd_output_state* output) {
    if(output->free_output) {
        output->free_output(output->data);
        output->data = NULL;
    }
}
