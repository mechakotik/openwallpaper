#ifdef WD_WLROOTS

#include "wlroots.h"
#include <SDL3/SDL.h>
#include <fcntl.h>
#include <sds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wlr-layer-shell-unstable-v1.h>
#include "error.h"
#include "hyprland.h"
#include "malloc.h"

#define WD_WLROOTS_MAX_OUTPUTS 32

typedef struct {
    uint32_t global_name;
    struct wl_output* output;
    sds name;
} output_data;

typedef struct wlroots_output_state {
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_compositor* compositor;
    struct zwlr_layer_shell_v1* layer_shell;

    output_data outputs[WD_WLROOTS_MAX_OUTPUTS];
    struct wl_output* target_output;
    const char* requested_output_name;

    struct wl_surface* surface;
    struct zwlr_layer_surface_v1* layer_surface;

    SDL_Window* window;
    uint32_t width;
    uint32_t height;
    bool window_closed;
    bool sdl_initialized;

    enum {
        SESSION_WLROOTS,
        SESSION_HYPRLAND,
    } session_type;

    wd_hyprland_state hyprland;
} wlroots_output_state;

static void output_geometry(void* data, struct wl_output* output, int32_t x, int32_t y, int32_t phy_width,
    int32_t phy_height, int32_t subpixel, const char* make, const char* model, int32_t transform) {}

static void output_mode(
    void* data, struct wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {}

static void output_done(void* data, struct wl_output* output) {}

static void output_scale(void* data, struct wl_output* output, int32_t factor) {}

static void output_name(void* data, struct wl_output* output, const char* name) {
    output_data* odata = (output_data*)data;
    sdsfree(odata->name);
    odata->name = sdsnew(name);
}

static void output_description(void* data, struct wl_output* output, const char* description) {}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void free_outputs(wlroots_output_state* state) {
    for(size_t i = 0; i < WD_WLROOTS_MAX_OUTPUTS; i++) {
        if(state->outputs[i].output != NULL) {
            wl_output_destroy(state->outputs[i].output);
            state->outputs[i].output = NULL;
        }
        sdsfree(state->outputs[i].name);
        state->outputs[i].name = NULL;
    }
}

static void add_output(wlroots_output_state* state, uint32_t name, uint32_t version) {
    output_data* output = NULL;
    for(size_t i = 0; i < WD_WLROOTS_MAX_OUTPUTS; i++) {
        if(state->outputs[i].output == NULL) {
            output = &state->outputs[i];
            break;
        }
    }

    if(output == NULL) {
        return;
    }

    *output = (output_data){0};

    output->global_name = name;
    output->output = wl_registry_bind(state->registry, name, &wl_output_interface, version);
    wl_output_add_listener(output->output, &output_listener, output);
}

static void registry_global(
    void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    if(strcmp(interface, wl_compositor_interface.name) == 0) {
        odata->compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        odata->layer_shell =
            (struct zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if(strcmp(interface, wl_output_interface.name) == 0) {
        add_output(odata, name, version);
    }
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
    (void)registry;

    wlroots_output_state* odata = (wlroots_output_state*)data;
    for(size_t i = 0; i < WD_WLROOTS_MAX_OUTPUTS; i++) {
        output_data* output = &odata->outputs[i];
        if(output->output == NULL || output->global_name != name) {
            continue;
        }

        if(odata->target_output == output->output) {
            odata->target_output = NULL;
            odata->window_closed = true;
        }

        if(output->output != NULL) {
            wl_output_destroy(output->output);
            output->output = NULL;
        }
        sdsfree(output->name);
        *output = (output_data){0};
        return;
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove};

static output_data* find_output_by_name(wlroots_output_state* odata, const char* name) {
    for(size_t i = 0; i < WD_WLROOTS_MAX_OUTPUTS; i++) {
        if(odata->outputs[i].output != NULL && odata->outputs[i].name != NULL &&
            strcmp(odata->outputs[i].name, name) == 0) {
            return &odata->outputs[i];
        }
    }
    return NULL;
}

static output_data* first_output(wlroots_output_state* odata) {
    for(size_t i = 0; i < WD_WLROOTS_MAX_OUTPUTS; i++) {
        if(odata->outputs[i].output != NULL) {
            return &odata->outputs[i];
        }
    }
    return NULL;
}

static bool select_output(wlroots_output_state* odata) {
    output_data* selected = NULL;
    if(odata->requested_output_name != NULL && odata->requested_output_name[0] != '\0') {
        selected = find_output_by_name(odata, odata->requested_output_name);
    } else {
        selected = first_output(odata);
    }

    if(selected == NULL) {
        odata->target_output = NULL;
        return false;
    }

    odata->target_output = selected->output;
    return true;
}

static const char* target_output_name(wlroots_output_state* odata) {
    if(odata->target_output == NULL) {
        return NULL;
    }

    for(size_t i = 0; i < WD_WLROOTS_MAX_OUTPUTS; i++) {
        if(odata->outputs[i].output != NULL && odata->outputs[i].output == odata->target_output) {
            return odata->outputs[i].name;
        }
    }
    return NULL;
}

static bool wlroots_connect(wlroots_output_state* odata) {
    odata->display = wl_display_connect(NULL);
    if(odata->display == NULL) {
        wd_set_error("failed to connect to wayland display");
        return false;
    }

    odata->registry = wl_display_get_registry(odata->display);
    wl_registry_add_listener(odata->registry, &registry_listener, odata);
    wl_display_roundtrip(odata->display);
    wl_display_roundtrip(odata->display);
    if(odata->compositor == NULL) {
        wd_set_error("compositor does not support wl_compositor");
        return false;
    }
    if(odata->layer_shell == NULL) {
        wd_set_error("compositor does not support wlr_layer_shell_v1");
        return false;
    }

    return true;
}

static void layer_surface_configure(
    void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height) {
    wlroots_output_state* state = (wlroots_output_state*)data;
    state->width = width;
    state->height = height;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    if(state->window != NULL) {
        SDL_SetWindowSize(state->window, width, height);
    }
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface) {
    (void)surface;

    wlroots_output_state* state = (wlroots_output_state*)data;
    state->window_closed = true;
    state->target_output = NULL;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure, .closed = layer_surface_closed};

static void deactivate_window(wlroots_output_state* state) {
    if(state->window != NULL) {
        SDL_DestroyWindow(state->window);
        state->window = NULL;
    }
    if(state->layer_surface != NULL) {
        zwlr_layer_surface_v1_destroy(state->layer_surface);
        state->layer_surface = NULL;
    }
    if(state->surface != NULL) {
        wl_surface_destroy(state->surface);
        state->surface = NULL;
    }
    state->width = 0;
    state->height = 0;
    state->window_closed = false;
}

static bool ensure_sdl_initialized(wlroots_output_state* state) {
    if(state->sdl_initialized) {
        return true;
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, state->display);
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        wd_set_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    state->sdl_initialized = true;
    return true;
}

static bool activate_output(wlroots_output_state* state) {
    if(state->target_output == NULL || state->window != NULL) {
        return true;
    }

    state->surface = wl_compositor_create_surface(state->compositor);
    state->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell, state->surface, state->target_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaperd");
    zwlr_layer_surface_v1_set_size(state->layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(
        state->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_set_exclusive_zone(state->layer_surface, -1);
    zwlr_layer_surface_v1_add_listener(state->layer_surface, &layer_surface_listener, state);
    wl_surface_commit(state->surface);
    wl_display_roundtrip(state->display);

    if(state->width == 0 || state->height == 0 || state->window_closed) {
        deactivate_window(state);
        return true;
    }
    if(!ensure_sdl_initialized(state)) {
        deactivate_window(state);
        return false;
    }

    SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, state->surface);
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, state->width);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, state->height);
    state->window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if(!state->window) {
        wd_set_error("SDL_CreateWindowWithProperties failed: %s", SDL_GetError());
        deactivate_window(state);
        return false;
    }

    SDL_ShowWindow(state->window);
    SDL_EnableScreenSaver();
    return true;
}

static void wlroots_free(wlroots_output_state* state, bool free_state) {
    if(state == NULL) {
        return;
    }

    deactivate_window(state);

    if(state->sdl_initialized) {
        SDL_Quit();
        state->sdl_initialized = false;
    }

    if(state->session_type == SESSION_HYPRLAND) {
        wd_hyprland_free(&state->hyprland);
    }

    if(state->layer_shell != NULL) {
        zwlr_layer_shell_v1_destroy(state->layer_shell);
        state->layer_shell = NULL;
    }
    if(state->compositor != NULL) {
        wl_compositor_destroy(state->compositor);
        state->compositor = NULL;
    }
    if(state->registry != NULL) {
        wl_registry_destroy(state->registry);
        state->registry = NULL;
    }
    free_outputs(state);
    if(state->display != NULL) {
        wl_display_disconnect(state->display);
        state->display = NULL;
    }

    if(free_state) {
        free(state);
    }
}

bool wd_wlroots_output_init(void** data, const char* display_name) {
    *data = wd_calloc(1, sizeof(wlroots_output_state));
    wlroots_output_state* state = (wlroots_output_state*)(*data);
    state->requested_output_name = display_name;

    if(!wlroots_connect(state)) {
        goto handle_error;
    }

    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if(desktop != NULL && strcmp(desktop, "Hyprland") == 0) {
        state->session_type = SESSION_HYPRLAND;
    } else {
        state->session_type = SESSION_WLROOTS;
    }

    if(state->session_type == SESSION_HYPRLAND) {
        wd_hyprland_init(&state->hyprland);
    }

    select_output(state);
    if(!activate_output(state)) {
        goto handle_error;
    }

    return true;

handle_error:
    wlroots_free(state, true);
    *data = NULL;
    return false;
}

bool wd_wlroots_list_displays() {
    wlroots_output_state state = {0};
    if(!wlroots_connect(&state)) {
        wlroots_free(&state, false);
        return false;
    }

    bool found = false;
    for(size_t i = 0; i < WD_WLROOTS_MAX_OUTPUTS; i++) {
        if(state.outputs[i].output == NULL) {
            continue;
        }
        const char* name = (state.outputs[i].name != NULL) ? state.outputs[i].name : "<unnamed>";
        printf("%s\n", name);
        found = true;
    }

    if(!found) {
        wd_set_error("no wlroots displays found");
        wlroots_free(&state, false);
        return false;
    }

    wlroots_free(&state, false);
    return true;
}

bool wd_wlroots_output_update(void* data) {
    wlroots_output_state* state = (wlroots_output_state*)data;
    if(state->window != NULL) {
        return true;
    }

    if(wl_display_roundtrip(state->display) < 0) {
        wd_set_error("wayland display roundtrip failed");
        return false;
    }
    if(state->target_output == NULL) {
        select_output(state);
    }
    if(state->target_output != NULL && !activate_output(state)) {
        return false;
    }

    return true;
}

static bool output_connected(wlroots_output_state* state) {
    return state->window != NULL && !state->window_closed && state->target_output != NULL;
}

SDL_Window* wd_wlroots_output_get_window(void* data) {
    wlroots_output_state* state = (wlroots_output_state*)data;
    return output_connected(state) ? state->window : NULL;
}

bool wd_wlroots_output_hidden(void* data) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    if(odata->session_type == SESSION_HYPRLAND) {
        return wd_hyprland_output_hidden(&odata->hyprland, target_output_name(odata));
    }

    // TODO: check for fullscreen window with wlr-foreign-toplevel-management
    // TODO: check whether the screen is turned off by idle daemon somehow
    return false;
}

void wd_wlroots_output_deactivate(void* data) {
    deactivate_window((wlroots_output_state*)data);
}

void wd_wlroots_output_free(void* data) {
    wlroots_free((wlroots_output_state*)data, true);
}

#endif
