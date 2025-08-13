#ifndef WD_ARGPARSE_H
#define WD_ARGPARSE_H

#include <stdbool.h>

typedef struct wd_args_state {
    char* wallpaper_path;

    int num_options;
    char** options_keys;
    char** options_values; // value is empty if string is unset

    int num_wallpaper_options;
    char** wallpaper_options_keys;
    char** wallpaper_options_values; // value is empty if string is unset
} wd_args_state;

bool wd_parse_args(wd_args_state* args, int argc, char* argv[]);
void wd_free_args(wd_args_state* args);

const char* wd_get_option(wd_args_state* args, const char* name);
const char* wd_get_wallpaper_path(wd_args_state* args);
const char* wd_get_wallpaper_option(wd_args_state* args, const char* name);

#endif
