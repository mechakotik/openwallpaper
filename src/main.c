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

    printf("  --output=<output>\n");
    printf("  --fps=<fps>\n");
    printf("  --prefer-dgpu\n");
    printf("  --pause-hidden\n");
    printf("  --pause-on-bat\n");
    printf("  --window\n");
    printf("  --help\n");
}

int main(int argc, char* argv[]) {
    wd_state state;
    wd_init_state(&state);
    wd_parse_args(&state.args, argc, argv);

    if(wd_get_option(&state.args, "help") != NULL) {
        print_help();
        wd_free_state(&state);
        return 0;
    }

    if(wd_get_wallpaper_path(&state.args) == NULL) {
        goto handle_error;
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
        frame_time = 1000000000 / fps;
    }

    uint64_t prev_time = SDL_GetTicksNS();

    bool pause_hidden = (wd_get_option(&state.args, "pause-hidden") != NULL);
    bool frame_skipped = false;
    uint64_t last_hidden_check = prev_time;
    bool first_draw = true;

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

        if(pause_hidden && !first_draw && last_hidden_check < cur_time - 2e8) {
            if(wd_output_hidden(&state.output)) {
                SDL_Delay(200);
                frame_skipped = true;
                continue;
            } else {
                last_hidden_check = cur_time;
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

        if(!wd_update_scene(&state.scene, delta)) {
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
