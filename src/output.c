#include "output.h"
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

static const char* get_output() {
    const char* output;
    if(wd_get_option("output") != NULL) {
        output = wd_get_option("output");
    } else {
        output = get_default_output();
    }
    return output;
}

static struct output_state {
    const char* output;
    struct SDL_Window* window;
    void (*free_output)();
} state = {0};

void wd_init_output() {
    state.output = get_output();

    if(strcmp(state.output, "window") == 0) {
        WD_LOG("using window output");
        wd_window_output_init();
        state.window = wd_window_output_get_window();
        state.free_output = wd_window_output_free;
    } else if(strcmp(state.output, "wlroots") == 0) {
#ifdef WD_WLROOTS
        WD_LOG("using wlroots output");
        wd_wlroots_output_init();
        state.window = wd_wlroots_output_get_window();
        state.free_output = wd_wlroots_output_free;
#else
        WD_ERROR("wlroots output support is disabled, compile with -DWD_WLROOTS=ON to use it");
#endif
    } else {
        WD_ERROR("unknown output '%s'", state.output);
    }
}

struct SDL_Window* wd_get_output_window() {
    return state.window;
}

void wd_free_output() {
    if(state.free_output != NULL) {
        state.free_output();
    }
}
