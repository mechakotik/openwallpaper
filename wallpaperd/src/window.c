#include "window.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include "error.h"
#include "malloc.h"

typedef struct window_output_data {
    SDL_Window* window;
} window_output_state;

bool wd_window_output_init(void** data, bool opengl) {
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        wd_set_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    if(opengl) {
        if(!SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1)) {
            wd_set_error("SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL) failed: %s", SDL_GetError());
            return false;
        }
        if(!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
            wd_set_error("SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER) failed: %s", SDL_GetError());
            return false;
        }
    }

    *data = wd_malloc(sizeof(window_output_state));
    window_output_state* odata = (window_output_state*)(*data);

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if(opengl) {
        flags |= SDL_WINDOW_OPENGL;
    }

    odata->window = SDL_CreateWindow("wallpaperd", 1280, 720, flags);
    if(odata->window == NULL) {
        wd_set_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

SDL_Window* wd_window_output_get_window(void* data) {
    window_output_state* odata = (window_output_state*)data;
    return odata->window;
}

void wd_window_output_free(void* data) {
    if(data == NULL) {
        return;
    }
    window_output_state* odata = (window_output_state*)data;
    if(odata->window != NULL) {
        SDL_DestroyWindow(odata->window);
    }
    SDL_Quit();
    free(data);
}
