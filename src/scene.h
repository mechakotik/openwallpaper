#ifndef WD_SCENE_H
#define WD_SCENE_H

#include <wasm_export.h>
#include "argparse.h"

typedef struct wd_scene_state {
    bool initialized;
    uint8_t* module_buffer;
    wasm_module_t module;
    wasm_module_inst_t instance;
    wasm_function_inst_t update_func;
    wasm_exec_env_t exec_env;
} wd_scene_state;

struct wd_state;

bool wd_init_scene(wd_scene_state* scene, wd_args_state* args);
bool wd_update_scene(struct wd_state* state);
void wd_free_scene(wd_scene_state* scene);

#endif
