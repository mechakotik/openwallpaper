#include "scene.h"
#include <lib_export.h>
#include <stdlib.h>
#include <wasm_export.h>
#include "error.h"
#include "state.h"
#include "wasm_api.h"
#include "zip.h"

static NativeSymbol native_symbols[] = {
    {"ow_log", ow_log, "(i)"},
    {"ow_load_file", ow_load_file, "(iii)"},
    {"ow_begin_copy_pass", ow_begin_copy_pass, "()"},
    {"ow_end_copy_pass", ow_end_copy_pass, "()"},
    {"ow_begin_render_pass", ow_begin_render_pass, "(i)"},
    {"ow_end_render_pass", ow_end_render_pass, "()"},
    {"ow_create_buffer", ow_create_buffer, "(ii)i"},
    {"ow_update_buffer", ow_update_buffer, "(iiii)"},
    {"ow_create_texture", ow_create_texture, "(i)i"},
    {"ow_create_texture_from_png", ow_create_texture_from_png, "(ii)i"},
    {"ow_update_texture", ow_update_texture, "(iii)"},
    {"ow_generate_mipmaps", ow_generate_mipmaps, "(i)"},
    {"ow_create_sampler", ow_create_sampler, "(i)i"},
    {"ow_create_shader_from_bytecode", ow_create_shader_from_bytecode, "(iii)i"},
    {"ow_create_shader_from_file", ow_create_shader_from_file, "(ii)i"},
    {"ow_create_pipeline", ow_create_pipeline, "(i)i"},
    {"ow_get_screen_size", ow_get_screen_size, "(ii)"},
    {"ow_push_uniform_data", ow_push_uniform_data, "(iiii)"},
    {"ow_render_geometry", ow_render_geometry, "(iiiii)"},
    {"ow_render_geometry_indexed", ow_render_geometry_indexed, "(iiiiii)"},
    {"ow_free", ow_free, "(i)"},
};

bool wd_init_scene(wd_state* state, wd_args_state* args) {
    wd_scene_state* scene = &state->scene;
    const uint32_t stack_size = 4 * 1024 * 1024;
    const uint32_t heap_size = 32 * 1024 * 1024;

    if(!wasm_runtime_init()) {
        wd_set_error("wasm_runtime_init failed");
        return false;
    }

    scene->initialized = true;
    int num_native_symbols = sizeof(native_symbols) / sizeof(NativeSymbol);
    if(!wasm_runtime_register_natives("env", native_symbols, num_native_symbols)) {
        wd_set_error("wasm_runtime_register_natives failed");
        return false;
    }

    const char* path = wd_get_wallpaper_path(args);
    if(!wd_init_zip(&state->zip, path)) {
        return false;
    }

    size_t size;
    if(!wd_read_from_zip(&state->zip, "scene.wasm", &scene->module_buffer, &size)) {
        return false;
    }

    char error_buf[128];
    scene->module = wasm_runtime_load(scene->module_buffer, size, error_buf, sizeof(error_buf));
    if(scene->module == NULL) {
        wd_set_error("wasm_runtime_load failed: %s", error_buf);
        return false;
    }

    scene->instance = wasm_runtime_instantiate(scene->module, stack_size, heap_size, error_buf, sizeof(error_buf));
    if(scene->instance == NULL) {
        wd_set_error("wasm_runtime_instantiate failed: %s", error_buf);
        return false;
    }

    wasm_runtime_set_custom_data(scene->instance, state);

    scene->exec_env = wasm_runtime_create_exec_env(scene->instance, stack_size);
    if(scene->exec_env == NULL) {
        wd_set_error("wasm_runtime_create_exec_env failed");
        return false;
    }

    wasm_function_inst_t init_func = wasm_runtime_lookup_function(scene->instance, "init");
    if(init_func == NULL) {
        wd_set_error("init function not found in wasm module");
        return false;
    }

    scene->update_func = wasm_runtime_lookup_function(scene->instance, "update");
    if(scene->update_func == NULL) {
        wd_set_error("update function not found in wasm module");
        return false;
    }

    if(!wasm_runtime_call_wasm(scene->exec_env, init_func, 0, NULL)) {
        return false;
    }

    return true;
}

bool wd_update_scene(wd_scene_state* scene, float delta) {
    if(!wasm_runtime_call_wasm(scene->exec_env, scene->update_func, 1, (void*)&delta)) {
        return false;
    }

    return true;
}

void wd_free_scene(wd_scene_state* scene) {
    if(scene->exec_env != NULL) {
        wasm_runtime_destroy_exec_env(scene->exec_env);
        scene->exec_env = NULL;
    }
    if(scene->instance != NULL) {
        wasm_runtime_deinstantiate(scene->instance);
        scene->instance = NULL;
    }
    if(scene->module != NULL) {
        wasm_runtime_unload(scene->module);
        scene->module = NULL;
    }
    if(scene->module_buffer != NULL) {
        free(scene->module_buffer);
        scene->module_buffer = NULL;
    }
    if(scene->initialized) {
        wasm_runtime_destroy();
        scene->initialized = false;
    }
}
