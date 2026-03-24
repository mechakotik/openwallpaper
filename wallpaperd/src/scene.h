#ifndef WD_SCENE_H
#define WD_SCENE_H

#include <wasm_export.h>

typedef struct wd_scene_state {
    bool wasm_initialized;
    uint8_t* module_buffer;
    wasm_module_t module;
    wasm_module_inst_t instance;
    wasm_function_inst_t update_func;
    wasm_exec_env_t exec_env;
    uint32_t* wallpaper_options_values_wasm;
    bool calling_init;

    struct SDL_GPUDevice* gpu;
    struct SDL_GPUCommandBuffer* command_buffer;
    struct SDL_GPUCopyPass* copy_pass;
    struct SDL_GPURenderPass* render_pass;
    struct SDL_GPUTexture* framebuffer;
    uint32_t width;
    uint32_t height;
} wd_scene_state;

struct wd_state;

bool wd_init_scene(struct wd_state* state);
bool wd_update_scene(struct wd_state* state, float delta);
void wd_free_scene(wd_scene_state* scene);

#endif
