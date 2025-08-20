#include "scene.h"
#include <bh_read_file.h>
#include "error.h"
#include "lib_export.h"
#include "state.h"
#include "wasm_export.h"

void ow_log_impl(wasm_exec_env_t exec_env, uint32_t message) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    const char* message_real = wasm_runtime_addr_app_to_native(instance, message);
    printf("%s\n", message_real);
}

static NativeSymbol native_symbols[] = {
    {"ow_log", ow_log_impl, "(i)"},
};

bool wd_init_scene(wd_scene_state* scene, wd_args_state* args) {
    const uint32_t stack_size = 8192;
    const uint32_t heap_size = 8192;

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
    uint32_t size;
    scene->module_buffer = (uint8_t*)bh_read_file_to_buffer(path, &size);
    if(scene->module_buffer == NULL) {
        wd_set_error("failed to read file '%s'", path);
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
        wd_set_error("init function failed: %s", wasm_runtime_get_exception(scene->instance));
        return false;
    }

    return true;
}

bool wd_update_scene(wd_state* state) {
    if(!wasm_runtime_call_wasm(state->scene.exec_env, state->scene.update_func, 0, NULL)) {
        wd_set_error("update function failed: %s", wasm_runtime_get_exception(state->scene.instance));
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
