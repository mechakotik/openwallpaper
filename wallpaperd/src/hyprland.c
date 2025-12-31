#ifdef WD_WLROOTS

#include "hyprland.h"
#include <SDL3/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr-layer-shell-unstable-v1.h>

void wd_hyprland_init(wd_hyprland_state* state) {
    state->socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if(state->socket == -1) {
        printf("warning: failed to create socket, pause-hidden will not work: %s\n", strerror(errno));
        return;
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

    res = connect(state->socket, (const struct sockaddr*)&name, sizeof(name));
    if(res == -1) {
        printf("warning: failed to connect to hyprland socket, pause-hidden will not work: %s\n", strerror(errno));
        goto handle_error;
    }

    int flags = fcntl(state->socket, F_GETFL, 0);
    if(flags < 0) {
        printf("warning: failed to get socket flags, pause-hidden will not work: %s\n", strerror(errno));
        goto handle_error;
    }
    if(fcntl(state->socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        printf("warning: failed to set O_NONBLOCK flag for socket, pause-hidden will not work: %s\n", strerror(errno));
        goto handle_error;
    }
    return;

handle_error:
    close(state->socket);
    state->socket = -1;
}

bool wd_hyprland_output_hidden(wd_hyprland_state* state) {
    if(state->socket == -1) {
        return false;
    }

    static char buf[128];
    bool refresh_needed = false;
    while(true) {
        int res = read(state->socket, buf, sizeof(buf));
        if(res < 0) {
            break;
        }
        refresh_needed = true;
    }
    if(!refresh_needed) {
        return state->output_hidden;
    }

    state->output_hidden = (system("hyprctl activewindow | grep -q \"floating: 0\\|fullscreen: 2\" > /dev/null") == 0);
    return state->output_hidden;
}

void wd_hyprland_free(wd_hyprland_state* state) {
    close(state->socket);
    state->socket = -1;
}

#endif
