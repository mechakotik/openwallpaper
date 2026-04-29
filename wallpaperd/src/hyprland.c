#include "hyprland.h"

#ifdef WD_WLROOTS

#include <SDL3/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <sds.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr-layer-shell-unstable-v1.h>
#include <yyjson.h>

typedef struct {
    sds data;
    yyjson_doc* doc;
} wd_hyprland_json;

typedef struct {
    int64_t monitor_id;
    int64_t active_workspace_id;
    int64_t special_workspace_id;
    bool has_special_workspace;
} wd_hyprland_monitor_info;

static sds read_command(const char* command) {
    FILE* file = popen(command, "r");
    if(file == NULL) {
        return NULL;
    }

    sds data = sdsempty();
    char buf[4096];

    while(true) {
        size_t read_len = fread(buf, 1, sizeof(buf), file);
        if(read_len > 0) {
            data = sdscatlen(data, buf, read_len);
        }
        if(read_len == 0) {
            break;
        }
    }

    bool read_ok = !ferror(file);
    int close_status = pclose(file);
    bool ok = read_ok && close_status == 0;
    if(!ok) {
        sdsfree(data);
        return NULL;
    }

    return data;
}

static bool read_json(const char* command, wd_hyprland_json* json) {
    *json = (wd_hyprland_json){0};
    json->data = read_command(command);
    if(json->data == NULL) {
        return false;
    }

    json->doc = yyjson_read(json->data, sdslen(json->data), 0);
    if(json->doc == NULL) {
        sdsfree(json->data);
        *json = (wd_hyprland_json){0};
        return false;
    }

    return true;
}

static void free_json(wd_hyprland_json* json) {
    yyjson_doc_free(json->doc);
    sdsfree(json->data);
    *json = (wd_hyprland_json){0};
}

static bool json_get_int(yyjson_val* object, const char* key, int64_t* value) {
    yyjson_val* val = yyjson_obj_get(object, key);
    if(!yyjson_is_int(val)) {
        return false;
    }

    *value = yyjson_get_sint(val);
    return true;
}

static bool find_monitor_info(yyjson_doc* doc, const char* output_name, wd_hyprland_monitor_info* info) {
    yyjson_val* monitors = yyjson_doc_get_root(doc);
    if(!yyjson_is_arr(monitors)) {
        return false;
    }

    size_t idx;
    size_t max;
    yyjson_val* monitor;
    yyjson_arr_foreach(monitors, idx, max, monitor) {
        yyjson_val* name = yyjson_obj_get(monitor, "name");
        if(!yyjson_is_str(name) || strcmp(yyjson_get_str(name), output_name) != 0) {
            continue;
        }

        if(!json_get_int(monitor, "id", &info->monitor_id)) {
            return false;
        }

        yyjson_val* active_workspace = yyjson_obj_get(monitor, "activeWorkspace");
        if(!yyjson_is_obj(active_workspace) || !json_get_int(active_workspace, "id", &info->active_workspace_id)) {
            return false;
        }

        info->has_special_workspace = false;
        yyjson_val* special_workspace = yyjson_obj_get(monitor, "specialWorkspace");
        yyjson_val* special_workspace_name = yyjson_obj_get(special_workspace, "name");
        if(yyjson_is_obj(special_workspace) && yyjson_is_str(special_workspace_name) &&
            yyjson_get_str(special_workspace_name)[0] != '\0') {
            info->has_special_workspace = json_get_int(special_workspace, "id", &info->special_workspace_id);
        }

        return true;
    }

    return false;
}

static bool client_on_workspace(yyjson_val* client, const wd_hyprland_monitor_info* info) {
    yyjson_val* workspace = yyjson_obj_get(client, "workspace");
    int64_t workspace_id;
    if(!yyjson_is_obj(workspace) || !json_get_int(workspace, "id", &workspace_id)) {
        return false;
    }

    return workspace_id == info->active_workspace_id ||
           (info->has_special_workspace && workspace_id == info->special_workspace_id);
}

static bool client_hides_output(yyjson_val* client, const wd_hyprland_monitor_info* info) {
    yyjson_val* mapped = yyjson_obj_get(client, "mapped");
    if(yyjson_is_bool(mapped) && !yyjson_get_bool(mapped)) {
        return false;
    }

    yyjson_val* hidden = yyjson_obj_get(client, "hidden");
    if(yyjson_is_bool(hidden) && yyjson_get_bool(hidden)) {
        return false;
    }

    int64_t monitor_id;
    if(!json_get_int(client, "monitor", &monitor_id) || monitor_id != info->monitor_id) {
        return false;
    }

    if(!client_on_workspace(client, info)) {
        return false;
    }

    yyjson_val* floating = yyjson_obj_get(client, "floating");
    yyjson_val* fullscreen = yyjson_obj_get(client, "fullscreen");
    return !yyjson_get_bool(floating) || yyjson_get_int(fullscreen) == 2;
}

static bool query_output_hidden(const char* output_name, bool* output_hidden) {
    if(output_name == NULL || output_name[0] == '\0') {
        return false;
    }

    wd_hyprland_json monitors;
    if(!read_json("hyprctl -j monitors", &monitors)) {
        return false;
    }

    wd_hyprland_monitor_info info = {0};
    bool ok = find_monitor_info(monitors.doc, output_name, &info);
    free_json(&monitors);
    if(!ok) {
        return false;
    }

    wd_hyprland_json clients;
    if(!read_json("hyprctl -j clients", &clients)) {
        return false;
    }

    yyjson_val* root = yyjson_doc_get_root(clients.doc);
    ok = yyjson_is_arr(root);
    *output_hidden = false;
    if(ok) {
        size_t idx;
        size_t max;
        yyjson_val* client;
        yyjson_arr_foreach(root, idx, max, client) {
            if(client_hides_output(client, &info)) {
                *output_hidden = true;
                break;
            }
        }
    }

    free_json(&clients);
    return ok;
}

void wd_hyprland_init(wd_hyprland_state* state) {
    state->output_hidden = false;
    state->output_hidden_valid = false;
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

bool wd_hyprland_output_hidden(wd_hyprland_state* state, const char* output_name) {
    if(state->socket == -1) {
        return false;
    }

    static char buf[128];
    bool refresh_needed = !state->output_hidden_valid;
    while(true) {
        ssize_t res = read(state->socket, buf, sizeof(buf));
        if(res <= 0) {
            break;
        }
        refresh_needed = true;
    }
    if(!refresh_needed) {
        return state->output_hidden;
    }

    if(query_output_hidden(output_name, &state->output_hidden)) {
        state->output_hidden_valid = true;
    }

    return state->output_hidden;
}

void wd_hyprland_free(wd_hyprland_state* state) {
    close(state->socket);
    state->socket = -1;
    state->output_hidden = false;
    state->output_hidden_valid = false;
}

#else

void wd_hyprland_init(wd_hyprland_state* state) {}

bool wd_hyprland_output_hidden(wd_hyprland_state* state, const char* output_name) {
    return false;
}

void wd_hyprland_free(wd_hyprland_state* state) {}

#endif
