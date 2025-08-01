#include "argparse.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

static struct {
    char* wallpaper_path;

    int num_options;
    char** options_keys;
    char** options_values; // value is empty if string is unset

    int num_wallpaper_options;
    char** wallpaper_options_keys;
    char** wallpaper_options_values; // value is empty if string is unset
} state;

static void split_option(const char* option, char** key, char** value) {
    int len = strlen(option);
    int pos = -1;
    for(int i = 0; i < len; i++) {
        if(option[i] != '=') {
            continue;
        }
        if(pos == -1) {
            pos = i;
        } else {
            WD_ERROR("option '%s' has multiple '='", option);
        }
    }

    if(pos == -1) {
        *key = malloc(sizeof(char) * (len + 1));
        *value = malloc(sizeof(char));
        strcpy(*key, option);
        *value[0] = '\0';
    } else {
        *key = malloc(sizeof(char) * (pos + 1));
        *value = malloc(sizeof(char) * (len - pos));
        strncpy(*key, option, pos);
        strncpy(*value, option + pos + 1, len - pos - 1);
        (*key)[pos] = (*value)[len - pos - 1] = '\0';
    }
}

void wd_parse_args(int argc, char* argv[]) {
    int num_options = 0;
    int num_wallpaper_options = 0;
    bool path_set = false;

    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] == '-') {
            if(path_set) {
                num_wallpaper_options++;
            } else {
                num_options++;
            }
        } else {
            if(path_set) {
                WD_ERROR("more than one wallpaper path provided, see --help");
            } else {
                int len = strlen(argv[i]);
                state.wallpaper_path = malloc(sizeof(char) * (len + 1));
                strcpy(state.wallpaper_path, argv[i]);
                path_set = true;
            }
        }
    }

    state.num_options = num_options;
    state.num_wallpaper_options = num_wallpaper_options;
    state.options_keys = calloc(state.num_options, sizeof(char*));
    state.options_values = calloc(state.num_options, sizeof(char*));
    state.wallpaper_options_keys = malloc(sizeof(char*) * state.num_wallpaper_options);
    state.wallpaper_options_values = malloc(sizeof(char*) * state.num_wallpaper_options);

    for(int i = 0; i < state.num_options; i++) {
        split_option(argv[i + 1] + 2, &state.options_keys[i], &state.options_values[i]);
    }
    for(int i = 0; i < state.num_wallpaper_options; i++) {
        split_option(argv[i + state.num_wallpaper_options + 2] + 2, &state.wallpaper_options_keys[i],
            &state.wallpaper_options_values[i]);
    }
}

void wd_free_args() {
    for(int i = 0; i < state.num_options; i++) {
        free(state.options_keys[i]);
        free(state.options_values[i]);
    }
    for(int i = 0; i < state.num_wallpaper_options; i++) {
        free(state.wallpaper_options_keys[i]);
        free(state.wallpaper_options_values[i]);
    }
    free(state.options_keys);
    free(state.options_values);
    free(state.wallpaper_path);
    free(state.wallpaper_options_keys);
    free(state.wallpaper_options_values);
}

const char* wd_get_option(const char* name) {
    for(int i = 0; i < state.num_options; i++) {
        if(strcmp(state.options_keys[i], name) == 0) {
            return state.options_values[i];
        }
    }
    return NULL;
}

const char* wd_get_wallpaper_path() {
    return state.wallpaper_path;
}

const char* wd_get_wallpaper_option(const char* name) {
    for(int i = 0; i < state.num_wallpaper_options; i++) {
        if(strcmp(state.wallpaper_options_keys[i], name) == 0) {
            return state.wallpaper_options_values[i];
        }
    }
    return NULL;
}
