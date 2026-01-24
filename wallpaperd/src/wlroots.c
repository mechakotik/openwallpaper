#include "malloc.h"
#ifdef WD_WLROOTS

#include <SDL3/SDL.h>
#include <fcntl.h>
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
#include "wlroots.h"

typedef struct {
    struct wl_output* output;
    char* name;
} output_data;

typedef struct wlroots_output_state {
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_compositor* compositor;
    struct zwlr_layer_shell_v1* layer_shell;

    output_data* outputs;
    size_t outputs_len;
    size_t outputs_cap;
    struct wl_output* target_output;
    const char* target_output_name;

    struct wl_surface* surface;
    struct zwlr_layer_surface_v1* layer_surface;

    SDL_Window* window;
    uint32_t width;
    uint32_t height;
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
    free(odata->name);
    odata->name = strdup(name);
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
    for(size_t i = 0; i < state->outputs_len; i++) {
        if(state->outputs[i].output != NULL) {
            wl_output_destroy(state->outputs[i].output);
            state->outputs[i].output = NULL;
        }
        free(state->outputs[i].name);
        state->outputs[i].name = NULL;
    }
    free(state->outputs);
    state->outputs = NULL;
    state->outputs_len = state->outputs_cap = 0;
}

static output_data* add_output(wlroots_output_state* state, uint32_t name, uint32_t version) {
    if(state->outputs_len == state->outputs_cap) {
        size_t new_cap = (state->outputs_cap == 0 ? 4 : state->outputs_cap * 2);
        output_data* new_outputs = realloc(state->outputs, sizeof(output_data) * new_cap);
        state->outputs = new_outputs;
        state->outputs_cap = new_cap;
    }

    output_data* output = &state->outputs[state->outputs_len];
    *output = (output_data){0};
    state->outputs_len++;

    output->output = wl_registry_bind(state->registry, name, &wl_output_interface, version);
    wl_output_add_listener(output->output, &output_listener, output);
    return output;
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

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove};

static output_data* find_output_by_name(wlroots_output_state* odata, const char* name) {
    for(size_t i = 0; i < odata->outputs_len; i++) {
        if(odata->outputs[i].name != NULL && strcmp(odata->outputs[i].name, name) == 0) {
            return &odata->outputs[i];
        }
    }
    return NULL;
}

static bool select_output(wlroots_output_state* odata, const char* requested) {
    if(odata->outputs_len == 0) {
        wd_set_error("no wlroots displays found");
        return false;
    }

    output_data* selected = &odata->outputs[0];
    if(requested != NULL && requested[0] != '\0') {
        output_data* matched = find_output_by_name(odata, requested);
        if(matched == NULL) {
            wd_set_error("display %s does not exist", requested);
            return false;
        }
        selected = matched;
    }

    odata->target_output = selected->output;
    odata->target_output_name = selected->name;
    return true;
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

    if(odata->outputs_len == 0) {
        wd_set_error("no wlroots displays found");
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

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface) {}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure, .closed = layer_surface_closed};

static void wlroots_free(wlroots_output_state* state, bool free_state) {
    if(state == NULL) {
        return;
    }

    if(state->sdl_initialized) {
        if(state->window != NULL) {
            SDL_DestroyWindow(state->window);
            state->window = NULL;
        }
        SDL_Quit();
        state->sdl_initialized = false;
    }

    if(state->session_type == SESSION_HYPRLAND) {
        wd_hyprland_free(&state->hyprland);
    }

    if(state->layer_surface != NULL) {
        zwlr_layer_surface_v1_destroy(state->layer_surface);
        state->layer_surface = NULL;
    }
    if(state->surface != NULL) {
        wl_surface_destroy(state->surface);
        state->surface = NULL;
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

    if(!wlroots_connect(state)) {
        goto handle_error;
    }
    if(!select_output(state, display_name)) {
        goto handle_error;
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

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, state->display);
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        wd_set_error("SDL_Init failed: %s", SDL_GetError());
        goto handle_error;
    }
    state->sdl_initialized = true;

    SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, state->surface);
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, state->width);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, state->height);
    state->window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if(!state->window) {
        wd_set_error("SDL_CreateWindowWithProperties failed: %s", SDL_GetError());
        goto handle_error;
    }

    SDL_ShowWindow(state->window);
    SDL_EnableScreenSaver();

    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if(desktop != NULL && strcmp(desktop, "Hyprland") == 0) {
        state->session_type = SESSION_HYPRLAND;
    } else {
        state->session_type = SESSION_WLROOTS;
    }

    if(state->session_type == SESSION_HYPRLAND) {
        wd_hyprland_init(&state->hyprland);
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

    for(size_t i = 0; i < state.outputs_len; i++) {
        const char* name = (state.outputs[i].name != NULL) ? state.outputs[i].name : "<unnamed>";
        printf("%s\n", name);
    }

    wlroots_free(&state, false);
    return true;
}

SDL_Window* wd_wlroots_output_get_window(void* data) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    return odata->window;
}

bool wd_wlroots_output_hidden(void* data) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    if(odata->session_type == SESSION_HYPRLAND) {
        return wd_hyprland_output_hidden(&odata->hyprland);
    }

    // TODO: check for fullscreen window with wlr-foreign-toplevel-management
    // TODO: check whether the screen is turned off by idle daemon somehow
    return false;
}

void wd_wlroots_output_free(void* data) {
    wlroots_free((wlroots_output_state*)data, true);
}

#endif
