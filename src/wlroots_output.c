#ifdef WD_WLROOTS

#include "wlroots_output.h"
#include <SDL3/SDL.h>
#include <wlr-layer-shell-unstable-v1.h>
#include "SDL3/SDL_hints.h"
#include "error.h"

static struct {
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_compositor* compositor;
    struct zwlr_layer_shell_v1* layer_shell;

    struct wl_surface* surface;
    struct zwlr_layer_surface_v1* layer_surface;

    SDL_Window* window;
    uint32_t width;
    uint32_t height;
} state;

static void registry_global(
    void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if(strcmp(interface, wl_compositor_interface.name) == 0) {
        state.compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state.layer_shell =
            (struct zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    }
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove};

static void layer_surface_configure(
    void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height) {
    state.width = width;
    state.height = height;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    if(state.window != NULL) {
        SDL_SetWindowSize(state.window, width, height);
    }
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface) {}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure, .closed = layer_surface_closed};

void wd_wlroots_output_init() {
    state.display = wl_display_connect(NULL);
    if(state.display == NULL) {
        WD_ERROR("failed to connect to wayland display");
    }

    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, NULL);
    wl_display_roundtrip(state.display);
    if(state.compositor == NULL) {
        WD_ERROR("compositor does not support wl_compositor");
    }
    if(state.layer_shell == NULL) {
        WD_ERROR("compositor does not support wlr_layer_shell_v1");
    }

    state.surface = wl_compositor_create_surface(state.compositor);
    state.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state.layer_shell, state.surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaperd");
    zwlr_layer_surface_v1_set_size(state.layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(
        state.layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                 ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_add_listener(state.layer_surface, &layer_surface_listener, &state);
    wl_surface_commit(state.surface);
    wl_display_roundtrip(state.display);

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, state.display);
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        WD_ERROR("SDL_Init failed: %s", SDL_GetError());
    }

    {
        SDL_PropertiesID properties = SDL_CreateProperties();
        SDL_SetPointerProperty(properties, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, state.surface);
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, state.width);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, state.height);
        state.window = SDL_CreateWindowWithProperties(properties);
        SDL_DestroyProperties(properties);
        if(!state.window) {
            WD_ERROR("SDL_CreateWindowWithProperties failed: %s", SDL_GetError());
        }
    }
}

SDL_Window* wd_wlroots_output_get_window() {
    return state.window;
}

void wd_wlroots_output_free() {
    SDL_DestroyWindow(state.window);
    SDL_Quit();

    if(state.layer_surface != NULL) {
        zwlr_layer_surface_v1_destroy(state.layer_surface);
    }
    if(state.surface != NULL) {
        wl_surface_destroy(state.surface);
    }
    if(state.layer_shell != NULL) {
        zwlr_layer_shell_v1_destroy(state.layer_shell);
    }
    if(state.compositor != NULL) {
        wl_compositor_destroy(state.compositor);
    }
    if(state.registry != NULL) {
        wl_registry_destroy(state.registry);
    }
    if(state.display != NULL) {
        wl_display_disconnect(state.display);
    }
}

#endif
