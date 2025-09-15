#ifdef WD_WLROOTS

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <wlr-layer-shell-unstable-v1.h>
#include "SDL3/SDL_hints.h"
#include "error.h"
#include "wlroots_output.h"

typedef struct wlroots_output_state {
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_compositor* compositor;
    struct zwlr_layer_shell_v1* layer_shell;

    struct wl_surface* surface;
    struct zwlr_layer_surface_v1* layer_surface;

    SDL_Window* window;
    uint32_t width;
    uint32_t height;
} wlroots_output_state;

static void registry_global(
    void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    if(strcmp(interface, wl_compositor_interface.name) == 0) {
        odata->compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        odata->layer_shell =
            (struct zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    }
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove};

static void layer_surface_configure(
    void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    odata->width = width;
    odata->height = height;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    if(odata->window != NULL) {
        SDL_SetWindowSize(odata->window, width, height);
    }
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface) {}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure, .closed = layer_surface_closed};

bool wd_wlroots_output_init(void** data) {
    *data = calloc(1, sizeof(wlroots_output_state));
    wlroots_output_state* odata = (wlroots_output_state*)(*data);

    odata->display = wl_display_connect(NULL);
    if(odata->display == NULL) {
        wd_set_error("failed to connect to wayland display");
        return false;
    }

    odata->registry = wl_display_get_registry(odata->display);
    wl_registry_add_listener(odata->registry, &registry_listener, odata);
    wl_display_roundtrip(odata->display);
    if(odata->compositor == NULL) {
        wd_set_error("compositor does not support wl_compositor");
        return false;
    }
    if(odata->layer_shell == NULL) {
        wd_set_error("compositor does not support wlr_layer_shell_v1");
        return false;
    }

    odata->surface = wl_compositor_create_surface(odata->compositor);
    odata->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        odata->layer_shell, odata->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaperd");
    zwlr_layer_surface_v1_set_size(odata->layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(
        odata->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_add_listener(odata->layer_surface, &layer_surface_listener, odata);
    wl_surface_commit(odata->surface);
    wl_display_roundtrip(odata->display);

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, odata->display);
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        wd_set_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, odata->surface);
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, odata->width);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, odata->height);
    odata->window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if(!odata->window) {
        wd_set_error("SDL_CreateWindowWithProperties failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

SDL_Window* wd_wlroots_output_get_window(void* data) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    return odata->window;
}

void wd_wlroots_output_free(void* data) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    SDL_DestroyWindow(odata->window);
    SDL_Quit();

    if(odata->layer_surface != NULL) {
        zwlr_layer_surface_v1_destroy(odata->layer_surface);
    }
    if(odata->surface != NULL) {
        wl_surface_destroy(odata->surface);
    }
    if(odata->layer_shell != NULL) {
        zwlr_layer_shell_v1_destroy(odata->layer_shell);
    }
    if(odata->compositor != NULL) {
        wl_compositor_destroy(odata->compositor);
    }
    if(odata->registry != NULL) {
        wl_registry_destroy(odata->registry);
    }
    if(odata->display != NULL) {
        wl_display_disconnect(odata->display);
    }
    free(odata);
}

#endif
