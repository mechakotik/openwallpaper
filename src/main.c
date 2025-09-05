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

    printf("  --output=<output>    set the output to use\n");
    printf("  --fps=<fps>          set target framerate, vsync if unspecified\n");
    printf("\n");
    printf("  --list-outputs       list available outputs and exit\n");
    printf("  --help               display help and exit\n");
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

    if(wd_get_option(&state.args, "list-outputs") != NULL) {
        wd_list_outputs();
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

    uint32_t prev_time = SDL_GetTicksNS();

    while(true) {
        if(fps != 0) {
            uint32_t delta = SDL_GetTicksNS() - prev_time;
            if(delta < frame_time) {
                SDL_DelayNS(frame_time - delta);
            }
        }

        uint32_t cur_time = SDL_GetTicksNS();
        float delta = (float)(cur_time - prev_time) / 1e9f;
        prev_time = cur_time;

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
    }

    wd_free_state(&state);
    return 0;

handle_error:
    if(wd_is_last_error_scene_error()) {
        printf("scene error: %s\n", wd_get_last_error());
    } else {
        printf("error: %s\n", wd_get_last_error());
    }
    wd_free_state(&state);
    return 1;
}
