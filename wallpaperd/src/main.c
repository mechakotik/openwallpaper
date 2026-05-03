#include <stdio.h>
#include <string.h>
#include "argparse.h"
#include "error.h"
#include "output.h"
#include "ready.h"
#include "scene.h"
#include "state.h"
#include "video.h"

static void print_help() {
    printf(
        "usage: wallpaperd [OPTIONS] [SCENE_PATH] [SCENE_OPTIONS]\n"
        "       wallpaperd [OPTIONS] [VIDEO_PATH]\n"
        "funny animated wallpaper app\n"
        "\n"
        "common options:\n"
        "  --display=<display>\n"
        "  --speed=<speed>\n"
        "  --pause-hidden\n"
        "  --pause-on-bat\n"
        "  --window\n"
        "  --list-displays\n"
        "  --help\n"
        "  --version\n"
        "\n"
        "scene-specific options:\n"
        "  --prefer-dgpu\n"
        "  --audio-backend=<backend>\n"
        "  --audio-source=<source>\n"
        "  --no-audio\n"
        "\n");
}

static bool is_scene(const char* path) {
    if(path == NULL) {
        return false;
    }

    const char* ext = strrchr(path, '.');
    return ext != NULL && strcmp(ext, ".owf") == 0;
}

int main(int argc, char* argv[]) {
    wd_unset_ready();

    wd_state state;
    wd_init_state(&state);
    if(!wd_parse_args(&state.args, argc, argv)) {
        goto handle_error;
    }

    if(state.args.help) {
        print_help();
        wd_free_state(&state);
        return 0;
    }

    if(state.args.version) {
        printf("wallpaperd 0.1.0\n");
        wd_free_state(&state);
        return 0;
    }

    if(state.args.list_displays) {
        if(!wd_list_displays(&state.args)) {
            goto handle_error;
        }
        wd_free_state(&state);
        return 0;
    }

    if(wd_get_wallpaper_path(&state.args) == NULL) {
        wd_set_error("no wallpaper path specified");
        goto handle_error;
    }

    bool scene_wallpaper = is_scene(wd_get_wallpaper_path(&state.args));
    bool opengl = !scene_wallpaper;

#ifndef WD_VIDEO
    if(!scene_wallpaper) {
        wd_set_error("video wallpaper support is disabled, rebuild with -DWD_VIDEO=ON");
        goto handle_error;
    }
#endif

    if(!wd_init_output(&state.output, &state.args, opengl)) {
        goto handle_error;
    }

    if(scene_wallpaper) {
        if(!wd_run_scene(&state)) {
            goto handle_error;
        }
    } else if(!wd_run_video(&state)) {
        goto handle_error;
    }

    wd_free_state(&state);
    wd_unset_ready();
    return 0;

handle_error:
    printf("error: %s\n", wd_get_last_error());
    wd_free_state(&state);
    wd_unset_ready();
    return 1;
}
