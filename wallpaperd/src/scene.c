#include "scene.h"
#include <SDL3/SDL.h>
#include <inttypes.h>
#include <lib_export.h>
#include <sds.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_export.h>
#include "cache.h"
#include "error.h"
#include "malloc.h"
#include "ready.h"
#include "state.h"
#include "wasm_api.h"
#include "zip.h"

#ifndef WD_WAMRC_BINARY
#define WD_WAMRC_BINARY "wamrc"
#endif

static NativeSymbol native_symbols[] = {
    {"ow_get_file_size", ow_get_file_size, "(i)i"},
    {"ow_read_file", ow_read_file, "(ii)"},
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
    const char* args[] = {WD_WAMRC_BINARY, "-o", aot_path, wasm_path, NULL};

    SDL_PropertiesID props = SDL_CreateProperties();
    if(props == 0) {
        printf("warning: failed to create wamrc process properties, using interpreter mode\n");
        return false;
    }

    if(!SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args) ||
        !SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, (Sint64)SDL_PROCESS_STDIO_NULL) ||
        !SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, (Sint64)SDL_PROCESS_STDIO_INHERITED) ||
        !SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true)) {
        printf("warning: failed to set wamrc process properties, using interpreter mode\n");
        SDL_DestroyProperties(props);
        return false;
    }

    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if(process == NULL) {
        printf("warning: failed to run wamrc (is it installed?), using interpreter mode\n");
        return false;
    }

    int exit_code = -255;
    if(SDL_WaitProcess(process, true, &exit_code) && exit_code == 0) {
        result = true;
    } else {
        printf("warning: wamrc failed (exit code %d), using interpreter mode\n", exit_code);
    }

    SDL_DestroyProcess(process);
    return result;
}

static bool compile_aot_to_cache(
    const char* cache_path, const char* cache_key, const uint8_t* wasm_buffer, size_t wasm_size) {
    bool result = false;

    sds tmp_dir = wd_cache_get_namespace_dir("tmp");
    if(tmp_dir == NULL) {
        printf("warning: failed to get tmp cache dir for AOT compilation, using interpreter mode\n");
        goto cleanup;
    }

    sds wasm_path = sdscatprintf(sdsdup(tmp_dir), "/%s.tmp-wasm", cache_key);
    sds aot_path = sdscatprintf(sdsdup(tmp_dir), "/%s.tmp-aot", cache_key);

    wd_cache_remove_file(wasm_path);
    wd_cache_remove_file(aot_path);

    if(!wd_cache_write_file(wasm_path, wasm_buffer, wasm_size)) {
        printf("warning: failed to write wasm file for AOT compilation, using interpreter mode\n");
        goto cleanup;
    }
    if(!run_wamrc(wasm_path, aot_path)) {
        goto cleanup;
    }

    SDL_PathInfo info;
    if(!SDL_GetPathInfo(aot_path, &info) || info.type != SDL_PATHTYPE_FILE || info.size == 0) {
        printf("warning: failed to find compiled AOT file, using interpreter mode\n");
        goto cleanup;
    }
    if(!SDL_RenamePath(aot_path, cache_path)) {
        printf("warning: failed to move compiled AOT file to cache, using interpreter mode\n");
        goto cleanup;
    }

    result = true;

cleanup:
    if(wasm_path != NULL) {
        wd_cache_remove_file(wasm_path);
        sdsfree(wasm_path);
    }
    if(!result && aot_path != NULL) {
        wd_cache_remove_file(aot_path);
    }
    sdsfree(aot_path);
    sdsfree(tmp_dir);
    return result;
}

static bool load_aot_module(
    wd_scene_state* scene, const uint8_t* wasm_buffer, size_t wasm_size, char* error_buf, size_t error_buf_size) {
    bool loaded = false;
    uint8_t* aot_buffer = NULL;
    size_t aot_size = 0;

    sds cache_dir = wd_cache_get_namespace_dir("aot");
    if(cache_dir == NULL) {
        printf("warning: failed to get AOT cache dir, using interpreter mode\n");
        goto cleanup;
    }

    sds key = sdscatprintf(sdsempty(), "%016" PRIx64, fnv1a64(wasm_buffer, wasm_size));
    sds cache_path = sdscatprintf(sdsdup(cache_dir), "/%s.aot", key);

    if(!wd_cache_read_file(cache_path, &aot_buffer, &aot_size)) {
        if(!compile_aot_to_cache(cache_path, key, wasm_buffer, wasm_size)) {
            goto cleanup;
        }
        if(!wd_cache_read_file(cache_path, &aot_buffer, &aot_size)) {
            printf("warning: failed to read compiled AOT module, using interpreter mode\n");
            goto cleanup;
        }
    }

    scene->module = wasm_runtime_load(aot_buffer, aot_size, error_buf, error_buf_size);
    if(scene->module == NULL) {
        printf("warning: failed to load AOT module, using interpreter mode\n");
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
    sdsfree(cache_path);
    sdsfree(key);
    sdsfree(cache_dir);
    return loaded;
}

static bool init_gpu(wd_state* state) {
    wd_scene_state* scene = &state->scene;
    wd_args_state* args = &state->args;

    SDL_PropertiesID gpu_properties = SDL_CreateProperties();
    if(!SDL_SetBooleanProperty(gpu_properties, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, !args->prefer_dgpu)) {
        printf("warning: failed to set preffered GPU: %s\n", SDL_GetError());
    }
    if(!SDL_SetBooleanProperty(gpu_properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true)) {
        wd_set_error("failed to enable SPIRV shaders: %s", SDL_GetError());
        return false;
    }

    scene->gpu = SDL_CreateGPUDeviceWithProperties(gpu_properties);
    if(scene->gpu == NULL) {
        wd_set_error("failed to create GPU device: %s", SDL_GetError());
        return false;
    }
    if(!SDL_ClaimWindowForGPUDevice(scene->gpu, state->output.window)) {
        wd_set_error("failed to claim window for GPU device: %s", SDL_GetError());
        return false;
    }
    scene->window = state->output.window;

    if(args->fps == 0) {
        bool ok = SDL_SetGPUSwapchainParameters(
            scene->gpu, state->output.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
        if(!ok) {
            printf("warning: failed to enable vsync: %s\n", SDL_GetError());
        }
    }

    return true;
}

static bool ensure_framebuffer(wd_state* state, uint32_t width, uint32_t height) {
    wd_scene_state* scene = &state->scene;

    if(scene->framebuffer != NULL && scene->width == width && scene->height == height) {
        return true;
    }
    if(scene->framebuffer != NULL) {
        SDL_ReleaseGPUTexture(scene->gpu, scene->framebuffer);
        scene->framebuffer = NULL;
    }

    SDL_GPUTextureCreateInfo texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GetGPUSwapchainTextureFormat(scene->gpu, state->output.window),
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    scene->framebuffer = SDL_CreateGPUTexture(scene->gpu, &texture_info);
    if(scene->framebuffer == NULL) {
        wd_set_error("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        return false;
    }

    scene->width = width;
    scene->height = height;
    return true;
}

bool init_wasm(wd_state* state) {
    wd_scene_state* scene = &state->scene;
    wd_args_state* args = &state->args;

    const uint32_t stack_size = 4 * 1024 * 1024;

    if(!wasm_runtime_init()) {
        wd_set_error("wasm_runtime_init failed");
        return false;
    }
    scene->wasm_initialized = true;

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
    if(!wd_zip_get_file_size(&state->zip, "scene.wasm", &module_size)) {
        return false;
    }

    scene->module_buffer = wd_malloc(module_size == 0 ? 1 : module_size);
    if(!wd_zip_read_file(&state->zip, "scene.wasm", scene->module_buffer)) {
        free(scene->module_buffer);
        scene->module_buffer = NULL;
        return false;
    }
    if(module_size > UINT32_MAX) {
        wd_set_error("scene.wasm is too large");
        return false;
    }
    if(wasm_runtime_get_file_package_type(scene->module_buffer, (uint32_t)module_size) != Wasm_Module_Bytecode) {
        wd_set_error("scene.wasm must be wasm bytecode");
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

    scene->calling_init = true;
    bool init_ok = wasm_runtime_call_wasm(scene->exec_env, init_func, 0, NULL);
    scene->calling_init = false;
    if(!init_ok) {
        if(!wd_is_error_set()) {
            wd_set_error("init wasm call failed: %s", wasm_runtime_get_exception(scene->instance));
        }
        return false;
    }

    return true;
}

static bool init_scene(wd_state* state) {
    wd_scene_state* scene = &state->scene;

    if(!init_gpu(state)) {
        return false;
    }

    scene->command_buffer = SDL_AcquireGPUCommandBuffer(scene->gpu);
    if(scene->command_buffer == NULL) {
        wd_set_error("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    if(!init_wasm(state)) {
        return false;
    }

    if(!SDL_SubmitGPUCommandBuffer(scene->command_buffer)) {
        wd_set_error("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

static bool update_scene(wd_state* state, float delta) {
    wd_scene_state* scene = &state->scene;

    scene->command_buffer = SDL_AcquireGPUCommandBuffer(scene->gpu);
    if(scene->command_buffer == NULL) {
        wd_set_error("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    int width = 0;
    int height = 0;
    if(!SDL_GetWindowSizeInPixels(state->output.window, &width, &height)) {
        wd_set_error("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        return false;
    }
    if(width <= 0 || height <= 0) {
        if(!SDL_SubmitGPUCommandBuffer(scene->command_buffer)) {
            wd_set_error("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            return false;
        }
        return true;
    }
    if(!ensure_framebuffer(state, (uint32_t)width, (uint32_t)height)) {
        return false;
    }

    if(!wasm_runtime_call_wasm(scene->exec_env, scene->update_func, 1, (void*)&delta)) {
        if(!wd_is_error_set()) {
            wd_set_error("update wasm call failed: %s", wasm_runtime_get_exception(scene->instance));
        }
        return false;
    }

    SDL_GPUTexture* swapchain_texture = NULL;
    uint32_t swapchain_width = 0;
    uint32_t swapchain_height = 0;

    if(!SDL_WaitAndAcquireGPUSwapchainTexture(
           scene->command_buffer, state->output.window, &swapchain_texture, &swapchain_width, &swapchain_height)) {
        wd_set_error("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return false;
    }

    if(swapchain_texture != NULL) {
        SDL_GPUBlitInfo blit_info = {
            .source =
                {
                    .texture = scene->framebuffer,
                    .w = scene->width,
                    .h = scene->height,
                },
            .destination =
                {
                    .texture = swapchain_texture,
                    .w = swapchain_width,
                    .h = swapchain_height,
                },
            .load_op = SDL_GPU_LOADOP_DONT_CARE,
            .flip_mode = SDL_FLIP_NONE,
            .filter = SDL_GPU_FILTER_LINEAR,
        };

        SDL_BlitGPUTexture(scene->command_buffer, &blit_info);
    }

    if(!SDL_SubmitGPUCommandBuffer(scene->command_buffer)) {
        wd_set_error("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool wd_flush_command_buffer(wd_scene_state* scene) {
    if(!SDL_SubmitGPUCommandBuffer(scene->command_buffer)) {
        wd_set_error("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    scene->command_buffer = SDL_AcquireGPUCommandBuffer(scene->gpu);
    if(scene->command_buffer == NULL) {
        wd_set_error("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

static void free_scene_with_deps(wd_state* state) {
    wd_free_object_manager(state);
    uint32_t unused;
    wd_new_object(&state->object_manager, WD_OBJECT_EMPTY, NULL, &unused);
    wd_free_scene(&state->scene);
    wd_free_zip(&state->zip);
}

bool wd_run_scene(wd_state* state) {
    if(!wd_init_audio_visualizer(&state->audio_visualizer, &state->args)) {
        return false;
    }

    float speed = 1;
    if(state->args.speed != 0) {
        speed = state->args.speed;
    }

    uint32_t fps = state->args.fps, frame_time = 0;
    if(fps != 0) {
        frame_time = 1000000000 / fps;
    }

    uint64_t prev_time = SDL_GetTicksNS();
    bool wallpaper_loaded = false;
    bool frame_skipped = false;
    uint64_t last_pause_check = prev_time;
    bool first_draw = true;
    float delta_factor = 1;

    if(state->args.pause_on_bat) {
        wd_init_battery(&state->battery);
    }

    while(true) {
        uint64_t cur_time = SDL_GetTicksNS();

        if(fps != 0) {
            uint64_t delta_ns = 0;
            if(cur_time > prev_time) {
                delta_ns = cur_time - prev_time;
            }
            if(delta_ns < frame_time) {
                SDL_DelayNS(frame_time - delta_ns);
            }
        }

        float delta = 0;
        if(!frame_skipped && cur_time > prev_time) {
            delta = (float)(cur_time - prev_time) / 1e9;
            if(delta > 1) {
                delta = 1;
            }
        }
        prev_time = cur_time;
        frame_skipped = false;

        SDL_Event event;
        bool quit = false;
        if(state->output.window != NULL) {
            while(SDL_PollEvent(&event)) {
                if(event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_TERMINATING) {
                    quit = true;
                    break;
                }
            }
        }
        if(quit) {
            break;
        }

        if(!wd_update_output(&state->output)) {
            return false;
        }
        if(state->output.window == NULL) {
            if(wallpaper_loaded) {
                free_scene_with_deps(state);
                wallpaper_loaded = false;
                wd_unset_ready();
                first_draw = true;
                last_pause_check = cur_time;
            }
            wd_deactivate_output(&state->output);
            SDL_Delay(200);
            frame_skipped = true;
            delta_factor = 0;
            continue;
        }
        if(!wallpaper_loaded) {
            if(!init_scene(state)) {
                return false;
            }
            wallpaper_loaded = true;
            first_draw = true;
            prev_time = SDL_GetTicksNS();
            last_pause_check = prev_time;
            delta_factor = 0;
            continue;
        }

        if(!first_draw && last_pause_check < cur_time - 200000000) {
            if((state->args.pause_hidden && wd_output_hidden(&state->output)) ||
                (state->args.pause_on_bat && wd_battery_discharging(&state->battery))) {
                SDL_Delay(200);
                frame_skipped = true;
                delta_factor = 0;
                continue;
            }
            last_pause_check = cur_time;
        }

        delta_factor += delta * 5;
        if(delta_factor > 1) {
            delta_factor = 1;
        }

        if(!update_scene(state, delta * delta_factor * speed)) {
            return false;
        }

        if(first_draw) {
            wd_set_ready();
            first_draw = false;
        }
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
    if(scene->wasm_initialized) {
        wasm_runtime_destroy();
        scene->wasm_initialized = false;
    }
    if(scene->framebuffer != NULL) {
        SDL_ReleaseGPUTexture(scene->gpu, scene->framebuffer);
        scene->framebuffer = NULL;
    }
    if(scene->gpu != NULL && scene->window != NULL) {
        SDL_ReleaseWindowFromGPUDevice(scene->gpu, scene->window);
        scene->window = NULL;
    }
    if(scene->gpu != NULL) {
        SDL_DestroyGPUDevice(scene->gpu);
        scene->gpu = NULL;
    }
    scene->command_buffer = NULL;
    scene->copy_pass = NULL;
    scene->render_pass = NULL;
    scene->width = 0;
    scene->height = 0;
}
