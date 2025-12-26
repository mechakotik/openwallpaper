#include "output.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_properties.h"
#include "argparse.h"
#include "error.h"
#include "window_output.h"
#include "wlroots_output.h"

static const char* get_output(wd_args_state* args) {
    if(wd_get_option(args, "window") != NULL) {
        return "window";
    }
    if(getenv("WAYLAND_DISPLAY") != NULL) {
        return "wlroots";
    }
    return "window";
}

bool wd_init_output(wd_output_state* output, wd_args_state* args) {
    const char* name = get_output(args);

    if(strcmp(name, "window") == 0) {
        if(!wd_window_output_init(&output->data)) {
            return false;
        }
        output->window = wd_window_output_get_window(output->data);
        output->free_output = wd_window_output_free;
    } else if(strcmp(name, "wlroots") == 0) {
#ifdef WD_WLROOTS
        if(!wd_wlroots_output_init(&output->data)) {
            return false;
        }
        output->window = wd_wlroots_output_get_window(output->data);
        output->output_hidden = wd_wlroots_output_hidden;
        output->free_output = wd_wlroots_output_free;
#else
        wd_set_error("wlroots output support is disabled, compile wallpaperd with -DWD_WLROOTS=ON to use it");
        return false;
#endif
    } else {
        wd_set_error("unknown output '%s'", name);
        return false;
    }

    SDL_PropertiesID gpu_properties = SDL_CreateProperties();
    bool prefer_dgpu = (wd_get_option(args, "prefer-dgpu") != NULL);
    if(!SDL_SetBooleanProperty(gpu_properties, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, !prefer_dgpu)) {
        printf("warning: failed to set preffered GPU: %s\n", SDL_GetError());
    }
    if(!SDL_SetBooleanProperty(gpu_properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true)) {
        wd_set_error("failed to enable SPIRV shaders: %s", SDL_GetError());
        return false;
    }

    output->gpu = SDL_CreateGPUDeviceWithProperties(gpu_properties);
    if(output->gpu == NULL) {
        wd_set_error("failed to create GPU device: %s", SDL_GetError());
        return false;
    }
    if(!SDL_ClaimWindowForGPUDevice(output->gpu, output->window)) {
        wd_set_error("failed to claim window for GPU device: %s", SDL_GetError());
        return false;
    }

    if(wd_get_option(args, "fps") == NULL) {
        bool ok = SDL_SetGPUSwapchainParameters(
            output->gpu, output->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
        if(!ok) {
            printf("warning: failed to enable vsync: %s\n", SDL_GetError());
        }
    }

    return true;
}

bool wd_output_hidden(wd_output_state* output) {
    if(output->output_hidden != NULL) {
        return output->output_hidden(output->data);
    }
    return false;
}

void wd_free_output(wd_output_state* output) {
    if(output->gpu != NULL) {
        SDL_DestroyGPUDevice(output->gpu);
        output->gpu = NULL;
    }
    if(output->free_output != NULL) {
        output->free_output(output->data);
        output->data = NULL;
    }
}
