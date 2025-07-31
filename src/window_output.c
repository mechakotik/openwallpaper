#include "window_output.h"
#include <SDL3/SDL.h>
#include "error.h"

static SDL_Window* window = NULL;

void wd_window_output_init() {
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        WD_ERROR("SDL_Init failed: %s", SDL_GetError());
    }

    window = SDL_CreateWindow("wallpaperd", 1280, 720, 0);
    if(window == NULL) {
        WD_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
    }
}

SDL_Window* wd_window_output_get_window() {
    return window;
}

void wd_window_output_free() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}
