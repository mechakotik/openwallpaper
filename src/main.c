#include <SDL3/SDL.h>
#include <stdio.h>
#include "argparse.h"
#include "error.h"
#include "output.h"
#include "state.h"

static void print_help() {
    printf("Usage: wallpaperd [OPTIONS] [WALLPAPER_PATH] [WALLPAPER_OPTIONS]\n");
    printf("Interactive live wallpaper daemon\n\n");

    printf("  --output=<output>    set the output to use\n");
    printf("  --fps=<fps>          set target framerate, default is 30\n");
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
        printf("error: no wallpaper path provided, see --help\n");
        wd_free_state(&state);
        return 1;
    }

    if(!wd_init_output(&state.output, &state.args)) {
        printf("%s", wd_get_last_error());
        wd_free_state(&state);
        return 1;
    }

    SDL_Window* window = state.output.window;
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    while(true) {
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

        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    wd_free_state(&state);
    return 0;
}
