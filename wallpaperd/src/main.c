#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_gpu.h"
#include "argparse.h"
#include "error.h"
#include "output.h"
#include "state.h"

static void print_help() {
    printf("Usage: wallpaperd [OPTIONS] [WALLPAPER_PATH] [WALLPAPER_OPTIONS]\n");
    printf("Interactive live wallpaper daemon\n\n");

    printf("  --display=<display>\n");
    printf("  --fps=<fps>\n");
    printf("  --speed=<speed>\n");
    printf("  --prefer-dgpu\n");
    printf("  --pause-hidden\n");
    printf("  --pause-on-bat\n");
    printf("  --window\n\n");

    printf("  --list-displays\n");
    printf("  --help\n");
}

int main(int argc, char* argv[]) {
    wd_state state;
    wd_init_state(&state);
    if(!wd_parse_args(&state.args, argc, argv)) {
        goto handle_error;
    }

    if(wd_get_option(&state.args, "help") != NULL) {
        print_help();
        wd_free_state(&state);
        return 0;
    }

    if(wd_get_option(&state.args, "list-displays") != NULL) {
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
    if(wd_get_option(&state.args, "speed") != NULL) {
        speed = atof(wd_get_option(&state.args, "speed"));
        if(speed <= 0) {
            wd_set_error("invalid speed value");
            goto handle_error;
        }
    }

    if(!wd_init_output(&state.output, &state.args)) {
        goto handle_error;
    }

    // It's unclear which component should manage the command buffer and swapchain texture,
    // so let's just do it in main.
    state.output.command_buffer = SDL_AcquireGPUCommandBuffer(state.output.gpu);
    if(state.output.command_buffer == NULL) {
        wd_set_error("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        goto handle_error;
    }
    if(!wd_init_scene(&state, &state.args)) {
        goto handle_error;
    }
    if(!SDL_SubmitGPUCommandBuffer(state.output.command_buffer)) {
        wd_set_error("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        goto handle_error;
    }

    uint32_t fps = 0, frame_time = 0;
    if(wd_get_option(&state.args, "fps") != NULL) {
        fps = atoi(wd_get_option(&state.args, "fps"));
        if(fps <= 0) {
            wd_set_error("invalid fps value");
            goto handle_error;
        }
        frame_time = 1000000000 / fps;
    }

    uint64_t prev_time = SDL_GetTicksNS();

    bool pause_hidden = (wd_get_option(&state.args, "pause-hidden") != NULL);
    bool pause_on_bat = (wd_get_option(&state.args, "pause-on-bat") != NULL);
    bool frame_skipped = false;
    uint64_t last_pause_check = prev_time;
    bool first_draw = true;

    if(pause_on_bat) {
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
            if(event.type == SDL_EVENT_QUIT) {
                quit = true;
                break;
            }
        }
        if(quit) {
            break;
        }

        if(!first_draw && last_pause_check < cur_time - 2e8) {
            if((pause_hidden && wd_output_hidden(&state.output)) ||
                (pause_on_bat && wd_battery_discharging(&state.battery))) {
                SDL_Delay(200);
                frame_skipped = true;
                continue;
            } else {
                last_pause_check = cur_time;
            }
        }

        state.output.command_buffer = SDL_AcquireGPUCommandBuffer(state.output.gpu);
        if(state.output.command_buffer == NULL) {
            wd_set_error("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
            goto handle_error;
        }

        if(!SDL_WaitAndAcquireGPUSwapchainTexture(state.output.command_buffer, state.output.window,
               &state.output.swapchain_texture, &state.output.width, &state.output.height)) {
            wd_set_error("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            goto handle_error;
        }

        if(state.output.swapchain_texture == NULL) {
            if(!SDL_SubmitGPUCommandBuffer(state.output.command_buffer)) {
                wd_set_error("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
                goto handle_error;
            }
            continue;
        }

        if(!wd_update_scene(&state.scene, delta * speed)) {
            goto handle_error;
        }

        if(!SDL_SubmitGPUCommandBuffer(state.output.command_buffer)) {
            wd_set_error("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            goto handle_error;
        }

        first_draw = false;
    }

    wd_free_state(&state);
    return 0;

handle_error:
    printf("error: %s\n", wd_get_last_error());
    wd_free_state(&state);
    return 1;
}
