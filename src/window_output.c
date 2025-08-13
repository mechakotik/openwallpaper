#include "window_output.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include "error.h"

typedef struct window_output_data {
    SDL_Window* window;
} window_output_data;

bool wd_window_output_init(void** data) {
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        wd_set_error("SDL_Init failed: %s", SDL_GetError());
    }

    *data = malloc(sizeof(window_output_data));
    window_output_data* odata = (window_output_data*)(*data);

    odata->window = SDL_CreateWindow("wallpaperd", 1280, 720, 0);
    if(odata->window == NULL) {
        wd_set_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

SDL_Window* wd_window_output_get_window(void* data) {
    window_output_data* odata = (window_output_data*)data;
    return odata->window;
}

void wd_window_output_free(void* data) {
    if(data == NULL) {
        return;
    }
    window_output_data* odata = (window_output_data*)data;
    if(odata->window != NULL) {
        SDL_DestroyWindow(odata->window);
    }
    SDL_Quit();
    free(data);
}
