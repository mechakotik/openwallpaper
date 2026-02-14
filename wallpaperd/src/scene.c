#include "scene.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <inttypes.h>
#include <lib_export.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_export.h>
#include "cache.h"
#include "error.h"
#include "malloc.h"
#include "state.h"
#include "wasm_api.h"
#include "zip.h"

static NativeSymbol native_symbols[] = {
    {"ow_load_file", ow_load_file, "(iii)"},
    {"ow_begin_copy_pass", ow_begin_copy_pass, "()"},
    {"ow_end_copy_pass", ow_end_copy_pass, "()"},
    {"ow_begin_render_pass", ow_begin_render_pass, "(i)"},
    {"ow_end_render_pass", ow_end_render_pass, "()"},
    {"ow_create_vertex_buffer", ow_create_vertex_buffer, "(i)i"},
    {"ow_create_index_buffer", ow_create_index_buffer, "(ii)i"},
    {"ow_update_vertex_buffer", ow_update_buffer, "(iiii)"},
    {"ow_update_index_buffer", ow_update_buffer, "(iiii)"},
    {"ow_create_texture", ow_create_texture, "(i)i"},
    {"ow_create_texture_from_image", ow_create_texture_from_image, "(ii)i"},
    {"ow_update_texture", ow_update_texture, "(iii)"},
    {"ow_generate_mipmaps", ow_generate_mipmaps, "(i)"},
    {"ow_create_sampler", ow_create_sampler, "(i)i"},
    {"ow_create_vertex_shader_from_bytecode", ow_create_vertex_shader_from_bytecode, "(ii)i"},
    {"ow_create_vertex_shader_from_file", ow_create_vertex_shader_from_file, "(i)i"},
    {"ow_create_fragment_shader_from_bytecode", ow_create_fragment_shader_from_bytecode, "(ii)i"},
    {"ow_create_fragment_shader_from_file", ow_create_fragment_shader_from_file, "(i)i"},
    {"ow_create_pipeline", ow_create_pipeline, "(i)i"},
    {"ow_push_vertex_uniform_data", ow_push_vertex_uniform_data, "(iii)"},
    {"ow_push_fragment_uniform_data", ow_push_fragment_uniform_data, "(iii)"},
    {"ow_render_geometry", ow_render_geometry, "(iiiii)"},
    {"ow_render_geometry_indexed", ow_render_geometry_indexed, "(iiiiii)"},
    {"ow_get_screen_size", ow_get_screen_size, "(ii)"},
    {"ow_get_mouse_state", ow_get_mouse_state, "(ii)i"},
    {"ow_get_audio_spectrum", ow_get_audio_spectrum, "(ii)"},
    {"ow_get_option", ow_get_option, "(i)i"},
    {"ow_free_vertex_buffer", ow_free, "(i)"},
    {"ow_free_index_buffer", ow_free, "(i)"},
    {"ow_free_texture", ow_free, "(i)"},
    {"ow_free_sampler", ow_free, "(i)"},
    {"ow_free_vertex_shader", ow_free, "(i)"},
    {"ow_free_fragment_shader", ow_free, "(i)"},
    {"ow_free_pipeline", ow_free, "(i)"},
};

static uint64_t fnv1a64(const uint8_t* data, size_t size) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for(size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool run_wamrc(const char* wasm_path, const char* aot_path) {
    bool result = false;
    const char* args[] = {"wamrc", "-o", aot_path, wasm_path, NULL};

    SDL_PropertiesID props = SDL_CreateProperties();
    if(props == 0) {
        return false;
    }

    if(!SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args) ||
        !SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, (Sint64)SDL_PROCESS_STDIO_NULL) ||
        !SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, (Sint64)SDL_PROCESS_STDIO_NULL)) {
        SDL_DestroyProperties(props);
        return false;
    }

    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if(process == NULL) {
        return false;
    }

    int exit_code = -255;
    if(SDL_WaitProcess(process, true, &exit_code) && exit_code == 0) {
        result = true;
    }

    SDL_DestroyProcess(process);
    return result;
}

static bool compile_aot_to_cache(
    const char* cache_path, const char* cache_key, const uint8_t* wasm_buffer, size_t wasm_size) {
    bool result = false;

    char tmp_dir[WD_MAX_PATH] = {0};
    if(!wd_cache_get_namespace_dir("tmp", tmp_dir, sizeof(tmp_dir))) {
        goto cleanup;
    }

    char wasm_path[WD_MAX_PATH] = {0};
    size_t ret = snprintf(wasm_path, sizeof(wasm_path), "%s/%s.tmp-wasm", tmp_dir, cache_key);
    if(ret < 0 || ret >= sizeof(wasm_path)) {
        goto cleanup;
    }

    char aot_path[WD_MAX_PATH] = {0};
    ret = snprintf(aot_path, sizeof(aot_path), "%s/%s.tmp-aot", tmp_dir, cache_key);
    if(ret < 0 || ret >= sizeof(aot_path)) {
        goto cleanup;
    }

    wd_cache_remove_file(wasm_path);
    wd_cache_remove_file(aot_path);

    if(!wd_cache_write_file(wasm_path, wasm_buffer, wasm_size)) {
        goto cleanup;
    }
    if(!run_wamrc(wasm_path, aot_path)) {
        goto cleanup;
    }

    SDL_PathInfo info;
    if(!SDL_GetPathInfo(aot_path, &info) || info.type != SDL_PATHTYPE_FILE || info.size == 0) {
        goto cleanup;
    }
    if(!SDL_RenamePath(aot_path, cache_path)) {
        goto cleanup;
    }

    result = true;

cleanup:
    if(wasm_path[0] != '\0') {
        wd_cache_remove_file(wasm_path);
    }
    if(!result && aot_path[0] != '\0') {
        wd_cache_remove_file(aot_path);
    }
    return result;
}

static bool load_aot_module(
    wd_scene_state* scene, const uint8_t* wasm_buffer, size_t wasm_size, char* error_buf, size_t error_buf_size) {
    bool loaded = false;
    uint8_t* aot_buffer = NULL;
    size_t aot_size = 0;

    char cache_dir[WD_MAX_PATH];
    if(!wd_cache_get_namespace_dir("aot", cache_dir, sizeof(cache_dir))) {
        goto cleanup;
    }

    char key[17];
    int ret = snprintf(key, sizeof(key), "%016" PRIx64, fnv1a64(wasm_buffer, wasm_size));
    if(ret < 0 || (size_t)ret >= sizeof(key)) {
        goto cleanup;
    }

    char cache_path[WD_MAX_PATH];
    ret = snprintf(cache_path, sizeof(cache_path), "%s/%s.aot", cache_dir, key);
    if(ret < 0 || ret >= sizeof(cache_path)) {
        goto cleanup;
    }

    if(!wd_cache_read_file(cache_path, &aot_buffer, &aot_size)) {
        if(!compile_aot_to_cache(cache_path, key, wasm_buffer, wasm_size)) {
            goto cleanup;
        }
        if(!wd_cache_read_file(cache_path, &aot_buffer, &aot_size)) {
            goto cleanup;
        }
    }

    scene->module = wasm_runtime_load(aot_buffer, aot_size, error_buf, error_buf_size);
    if(scene->module == NULL) {
        wd_cache_remove_file(cache_path);
        goto cleanup;
    }

    scene->module_buffer = aot_buffer;
    aot_buffer = NULL;
    loaded = true;

cleanup:
    if(aot_buffer != NULL) {
        free(aot_buffer);
    }
    return loaded;
}

bool wd_init_scene(wd_state* state, wd_args_state* args) {
    wd_scene_state* scene = &state->scene;
    const uint32_t stack_size = 4 * 1024 * 1024;

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

    size_t module_size = 0;
    if(!wd_read_from_zip(&state->zip, "scene.wasm", &scene->module_buffer, &module_size)) {
        return false;
    }

    char error_buf[128];
    uint8_t* wasm_buffer = scene->module_buffer;
    if(load_aot_module(scene, wasm_buffer, module_size, error_buf, sizeof(error_buf))) {
        free(wasm_buffer);
    } else {
        scene->module = wasm_runtime_load(scene->module_buffer, module_size, error_buf, sizeof(error_buf));
        if(scene->module == NULL) {
            wd_set_error("wasm_runtime_load failed: %s", error_buf);
            return false;
        }
    }

    scene->instance = wasm_runtime_instantiate(scene->module, stack_size, 0, error_buf, sizeof(error_buf));
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

    scene->wallpaper_options_values_wasm = wd_malloc(sizeof(uint32_t) * args->num_wallpaper_options);
    for(int i = 0; i < args->num_wallpaper_options; i++) {
        const char* value = args->wallpaper_options_values[i];
        void* value_wasm_native = NULL;
        uint32_t value_wasm =
            wasm_runtime_module_malloc(scene->instance, sizeof(char) * (strlen(value) + 1), &value_wasm_native);
        if(value_wasm == 0) {
            wd_set_error("failed to allocate wasm memory for wallpaper option value");
            return false;
        }
        memcpy(value_wasm_native, value, sizeof(char) * (strlen(value) + 1));
        scene->wallpaper_options_values_wasm[i] = value_wasm;
    }

    if(!wasm_runtime_call_wasm(scene->exec_env, init_func, 0, NULL)) {
        if(!wd_is_error_set()) {
            wd_set_error("init wasm call failed: %s", wasm_runtime_get_exception(scene->instance));
        }
        return false;
    }

    return true;
}

bool wd_update_scene(wd_scene_state* scene, float delta) {
    if(!wasm_runtime_call_wasm(scene->exec_env, scene->update_func, 1, (void*)&delta)) {
        if(!wd_is_error_set()) {
            wd_set_error("update wasm call failed: %s", wasm_runtime_get_exception(scene->instance));
        }
        return false;
    }

    return true;
}

void wd_free_scene(wd_scene_state* scene) {
    if(scene->wallpaper_options_values_wasm != NULL) {
        free(scene->wallpaper_options_values_wasm);
        scene->wallpaper_options_values_wasm = NULL;
    }
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
