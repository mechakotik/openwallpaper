#ifdef WD_WLROOTS

#include "wlroots_output.h"
#include <SDL3/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr-layer-shell-unstable-v1.h>
#include "SDL3/SDL_hints.h"
#include "error.h"

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

    enum {
        SESSION_WLROOTS,
        SESSION_HYPRLAND,
    } session_type;

    int hyprland_socket;
    bool hyprland_output_hidden;
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

static int init_hyprland_socket() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sock == -1) {
        printf("warning: failed to create socket, pause-hidden will not work: %s\n", strerror(errno));
        return -1;
    }

    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    if(runtime_dir == NULL) {
        printf("warning: XDG_RUNTIME_DIR is not set, pause-hidden will not work\n");
        goto handle_error;
    }
    const char* hyprland_instance = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if(hyprland_instance == NULL) {
        printf("warning: HYPRLAND_INSTANCE_SIGNATURE is not set, pause-hidden will not work\n");
        goto handle_error;
    }

    struct sockaddr_un name;
    name.sun_family = AF_UNIX;
    int res =
        snprintf(name.sun_path, sizeof(name.sun_path), "%s/hypr/%s/.socket2.sock", runtime_dir, hyprland_instance);
    if(res == sizeof(name.sun_path)) {
        printf("warning: socket path is too long, pause-hidden will not work: %s\n", strerror(errno));
        goto handle_error;
    }

    res = connect(sock, (const struct sockaddr*)&name, sizeof(name));
    if(res == -1) {
        printf("warning: failed to connect to hyprland socket, pause-hidden will not work: %s\n", strerror(errno));
        goto handle_error;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if(flags < 0) {
        printf("warning: failed to get socket flags, pause-hidden will not work: %s\n", strerror(errno));
        goto handle_error;
    }
    if(fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        printf("warning: failed to set O_NONBLOCK flag for socket, pause-hidden will not work: %s\n", strerror(errno));
        goto handle_error;
    }

    return sock;

handle_error:
    close(sock);
    return -1;
}

bool wd_wlroots_output_init(void** data) {
    *data = calloc(1, sizeof(wlroots_output_state));
    wlroots_output_state* odata = (wlroots_output_state*)(*data);
    odata->hyprland_socket = -1;

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
    zwlr_layer_surface_v1_set_exclusive_zone(odata->layer_surface, -1);
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
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, odata->width);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, odata->height);
    odata->window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if(!odata->window) {
        wd_set_error("SDL_CreateWindowWithProperties failed: %s", SDL_GetError());
        return false;
    }

    SDL_ShowWindow(odata->window);
    SDL_EnableScreenSaver();

    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if(desktop != NULL && strcmp(desktop, "Hyprland") == 0) {
        odata->session_type = SESSION_HYPRLAND;
    } else {
        odata->session_type = SESSION_WLROOTS;
    }

    if(odata->session_type == SESSION_HYPRLAND) {
        odata->hyprland_socket = init_hyprland_socket();
    }

    return true;
}

SDL_Window* wd_wlroots_output_get_window(void* data) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    return odata->window;
}

bool hyprland_output_hidden(wlroots_output_state* state) {
    if(state->hyprland_socket == -1) {
        return false;
    }

    // TODO: actually use information from hyprland socket to avoid calling shell
    static char buf[128];
    bool refresh_needed = false;
    while(true) {
        int res = read(state->hyprland_socket, buf, sizeof(buf));
        if(res < 0) {
            break;
        }
        refresh_needed = true;
    }
    if(!refresh_needed) {
        return state->hyprland_output_hidden;
    }

    state->hyprland_output_hidden =
        (system("hyprctl activewindow | grep -q \"floating: 0\\|fullscreen: 2\" > /dev/null") == 0);
    return state->hyprland_output_hidden;
}

bool wd_wlroots_output_hidden(void* data) {
    wlroots_output_state* odata = (wlroots_output_state*)data;
    if(odata->session_type == SESSION_HYPRLAND) {
        return hyprland_output_hidden(odata);
    }

    // TODO: check for fullscreen window with wlr-foreign-toplevel-management
    // TODO: check whether the screen is turned off by idle daemon somehow
    return false;
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
