#include "argparse.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

static struct args {
    char* wallpaper_path;

    int num_options;
    char** options_keys;
    char** options_values; // value is empty string if unset

    int num_wallpaper_options;
    char** wallpaper_options_keys;
    char** wallpaper_options_values; // value is empty string if unset
} args = {
    .wallpaper_path = NULL,

    .num_options = 0,
    .options_keys = NULL,
    .options_values = NULL,

    .num_wallpaper_options = 0,
    .wallpaper_options_keys = NULL,
    .wallpaper_options_values = NULL,
};

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
    }
}

void wd_parse_args(int argc, char* argv[]) {
    bool path_set = false;
    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] == '-') {
            if(path_set) {
                args.num_wallpaper_options++;
            } else {
                args.num_options++;
            }
        } else {
            if(path_set) {
                WD_ERROR("more than one wallpaper path provided, see --help");
            } else {
                int len = strlen(argv[i]);
                args.wallpaper_path = malloc(sizeof(char) * (len + 1));
                strcpy(args.wallpaper_path, argv[i]);
                path_set = true;
            }
        }
    }

    args.options_keys = malloc(sizeof(char*) * args.num_options);
    args.options_values = malloc(sizeof(char*) * args.num_options);
    args.wallpaper_options_keys = malloc(sizeof(char*) * args.num_wallpaper_options);
    args.wallpaper_options_values = malloc(sizeof(char*) * args.num_wallpaper_options);

    for(int i = 0; i < args.num_options; i++) {
        split_option(argv[i + 1] + 2, &args.options_keys[i], &args.options_values[i]);
    }
    for(int i = 0; i < args.num_wallpaper_options; i++) {
        split_option(argv[i + args.num_wallpaper_options + 2] + 2, &args.wallpaper_options_keys[i],
            &args.wallpaper_options_values[i]);
    }
}

void wd_free_args() {
    for(int i = 0; i < args.num_options; i++) {
        free(args.options_keys[i]);
        free(args.options_values[i]);
    }
    for(int i = 0; i < args.num_wallpaper_options; i++) {
        free(args.wallpaper_options_keys[i]);
        free(args.wallpaper_options_values[i]);
    }
    free(args.options_keys);
    free(args.options_values);
    free(args.wallpaper_path);
    free(args.wallpaper_options_keys);
    free(args.wallpaper_options_values);
}

const char* wd_get_option(const char* name) {
    for(int i = 0; i < args.num_options; i++) {
        if(strcmp(args.options_keys[i], name) == 0) {
            return args.options_values[i];
        }
    }
    return NULL;
}

const char* wd_get_wallpaer_path() {
    return args.wallpaper_path;
}

const char* wd_get_wallpaper_option(const char* name) {
    for(int i = 0; i < args.num_wallpaper_options; i++) {
        if(strcmp(args.wallpaper_options_keys[i], name) == 0) {
            return args.wallpaper_options_values[i];
        }
    }
    return NULL;
}
