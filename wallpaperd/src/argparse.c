#include "argparse.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

static bool split_option(const char* option, char** key, char** value) {
    int len = strlen(option);
    int pos = -1;
    for(int i = 0; i < len; i++) {
        if(option[i] != '=') {
            continue;
        }
        if(pos == -1) {
            pos = i;
        } else {
            wd_set_error("option '%s' has multiple '='", option);
            return false;
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

    return true;
}

bool wd_parse_args(wd_args_state* args, int argc, char* argv[]) {
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
                wd_set_error("more than one wallpaper path provided, see --help");
                return false;
            } else {
                int len = strlen(argv[i]);
                args->wallpaper_path = malloc(sizeof(char) * (len + 1));
                strcpy(args->wallpaper_path, argv[i]);
                path_set = true;
            }
        }
    }

    args->num_options = num_options;
    args->num_wallpaper_options = num_wallpaper_options;
    args->options_keys = calloc(args->num_options, sizeof(char*));
    args->options_values = calloc(args->num_options, sizeof(char*));
    args->wallpaper_options_keys = calloc(args->num_wallpaper_options, sizeof(char*));
    args->wallpaper_options_values = calloc(args->num_wallpaper_options, sizeof(char*));

    for(int i = 0; i < args->num_options; i++) {
        if(!split_option(argv[i + 1] + 2, &args->options_keys[i], &args->options_values[i])) {
            return false;
        }
    }
    for(int i = 0; i < args->num_wallpaper_options; i++) {
        if(!split_option(argv[i + args->num_options + 2] + 2, &args->wallpaper_options_keys[i],
               &args->wallpaper_options_values[i])) {
            return false;
        }
    }

    return true;
}

void wd_free_args(wd_args_state* args) {
    for(int i = 0; i < args->num_options; i++) {
        free(args->options_keys[i]);
        free(args->options_values[i]);
    }
    for(int i = 0; i < args->num_wallpaper_options; i++) {
        free(args->wallpaper_options_keys[i]);
        free(args->wallpaper_options_values[i]);
    }
    free(args->options_keys);
    free(args->options_values);
    free(args->wallpaper_path);
    free(args->wallpaper_options_keys);
    free(args->wallpaper_options_values);
}

const char* wd_get_option(wd_args_state* args, const char* name) {
    for(int i = 0; i < args->num_options; i++) {
        if(strcmp(args->options_keys[i], name) == 0) {
            return args->options_values[i];
        }
    }
    return NULL;
}

const char* wd_get_wallpaper_path(wd_args_state* args) {
    return args->wallpaper_path;
}

const char* wd_get_wallpaper_option(wd_args_state* args, const char* name) {
    for(int i = 0; i < args->num_wallpaper_options; i++) {
        if(strcmp(args->wallpaper_options_keys[i], name) == 0) {
            return args->wallpaper_options_values[i];
        }
    }
    return NULL;
}
