#ifndef WD_OUTPUT_H
#define WD_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "argparse.h"

typedef struct wd_output_state {
    struct SDL_Window* window;
    struct SDL_GPUDevice* gpu;
    struct SDL_GPUCommandBuffer* command_buffer;
    struct SDL_GPUCopyPass* copy_pass;
    struct SDL_GPURenderPass* render_pass;
    struct SDL_GPUTexture* swapchain_texture;
    uint32_t width;
    uint32_t height;

    void* data;
    bool (*output_hidden)(void*);
    void (*free_output)(void*);
} wd_output_state;

bool wd_init_output(wd_output_state* output, wd_args_state* args);
bool wd_output_hidden(wd_output_state* output);
void wd_free_output(wd_output_state* output);

#endif
