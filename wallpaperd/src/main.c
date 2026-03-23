#include <SDL3/SDL.h>
#include <stdio.h>
#include "argparse.h"
#include "error.h"
#include "output.h"
#include "ready.h"
#include "state.h"

static void print_help() {
    printf("usage: wallpaperd [OPTIONS] [WALLPAPER_PATH] [WALLPAPER_OPTIONS]\n");
    printf("funny animated wallpaper app\n\n");

    printf("  --display=<display>\n");
    printf("  --fps=<fps>\n");
    printf("  --speed=<speed>\n");
    printf("  --prefer-dgpu\n");
    printf("  --pause-hidden\n");
    printf("  --pause-on-bat\n");
    printf("  --audio-backend=<backend>\n");
    printf("  --audio-source=<source>\n");
    printf("  --no-audio\n");
    printf("  --window\n\n");

    printf("  --list-displays\n");
    printf("  --help\n");
    printf("  --version\n");
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

    float speed = 1;
    if(state.args.speed != 0) {
        speed = state.args.speed;
    }

    if(!wd_init_audio_visualizer(&state.audio_visualizer, &state.args)) {
        goto handle_error;
    }
    if(!wd_init_output(&state.output, &state.args)) {
        goto handle_error;
    }
    if(!wd_init_scene(&state)) {
        goto handle_error;
    }

    uint32_t fps = state.args.fps, frame_time = 0;
    if(fps != 0) {
        frame_time = 1000000000 / fps;
    }

    uint64_t prev_time = SDL_GetTicksNS();

    bool frame_skipped = false;
    uint64_t last_pause_check = prev_time;
    bool first_draw = true;
    float delta_factor = 1;

    if(state.args.pause_on_bat) {
        wd_init_battery(&state.battery);
    }

    while(true) {
        uint64_t cur_time = SDL_GetTicksNS();

        if(fps != 0) {
            uint64_t delta = 0;
            if(cur_time > prev_time) {
                delta = cur_time - prev_time;
            }
            if(delta < frame_time) {
                SDL_DelayNS(frame_time - delta);
            }
        }

        float delta = 0;
        if(!frame_skipped && cur_time > prev_time) {
            delta = (float)(cur_time - prev_time) / 1e9;
            if(delta > 1) {
                delta = 1;
            }
        }
        prev_time = cur_time;
        frame_skipped = false;

        SDL_Event event;
        bool quit = false;
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_TERMINATING) {
                quit = true;
                break;
            }
        }
        if(quit) {
            break;
        }

        if(!first_draw && last_pause_check < cur_time - 2e8) {
            if((state.args.pause_hidden && wd_output_hidden(&state.output)) ||
                (state.args.pause_on_bat && wd_battery_discharging(&state.battery))) {
                SDL_Delay(200);
                frame_skipped = true;
                delta_factor = 0;
                continue;
            }
            last_pause_check = cur_time;
        }

        delta_factor += delta * 5;
        if(delta_factor > 1) {
            delta_factor = 1;
        }

        if(!wd_update_scene(&state, delta * delta_factor * speed)) {
            goto handle_error;
        }

        if(first_draw) {
            wd_set_ready();
            first_draw = false;
        }
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
