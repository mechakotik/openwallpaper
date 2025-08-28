#include "output.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_properties.h"
#include "argparse.h"
#include "error.h"
#include "window_output.h"
#include "wlroots_output.h"

void wd_list_outputs() {
    printf("window");
#ifdef WD_WLROOTS
    printf(" wlroots");
#endif
    printf("\n");
}

static const char* get_default_output() {
    return "window";
}

static const char* get_output(wd_args_state* args) {
    const char* output;
    if(wd_get_option(args, "output") != NULL) {
        output = wd_get_option(args, "output");
    } else {
        output = get_default_output();
    }
    return output;
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
        output->free_output = wd_wlroots_output_free;
#else
        wd_set_error("wlroots output support is disabled, compile with -DWD_WLROOTS=ON to use it");
        return false;
#endif
    } else {
        wd_set_error("unknown output '%s'", name);
        return false;
    }

    SDL_PropertiesID gpu_properties = SDL_CreateProperties();
    SDL_SetBooleanProperty(gpu_properties, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, true);
    SDL_SetBooleanProperty(gpu_properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
    SDL_SetBooleanProperty(gpu_properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    output->gpu = SDL_CreateGPUDeviceWithProperties(gpu_properties);
    SDL_ClaimWindowForGPUDevice(output->gpu, output->window);
    SDL_SetGPUSwapchainParameters(
        output->gpu, output->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    return true;
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
