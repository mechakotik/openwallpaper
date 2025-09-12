#include "wasm_api.h"
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <stdio.h>
#include <stdlib.h>
#include "SDL3/SDL_gpu.h"
#include "error.h"
#include "object_manager.h"
#include "state.h"
#include "wasm_export.h"

#define DEBUG_CHECK(x, ...)                       \
    if(!(x)) {                                    \
        wd_set_scene_error(__VA_ARGS__);          \
        wasm_runtime_set_exception(instance, ""); \
        return;                                   \
    }

#define DEBUG_CHECK_RET0(x, ...)                  \
    if(!(x)) {                                    \
        wd_set_scene_error(__VA_ARGS__);          \
        wasm_runtime_set_exception(instance, ""); \
        return 0;                                 \
    }

void ow_log(wasm_exec_env_t exec_env, uint32_t message_ptr) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    const char* message_ptr_real = wasm_runtime_addr_app_to_native(instance, message_ptr);
    printf("%s\n", message_ptr_real);
}

void ow_begin_copy_pass(wasm_exec_env_t exec_env) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);

    DEBUG_CHECK(state->output.copy_pass == NULL, "called ow_begin_copy_pass when copy pass is active");
    DEBUG_CHECK(state->output.render_pass == NULL, "called ow_begin_copy_pass when render pass is active");

    state->output.copy_pass = SDL_BeginGPUCopyPass(state->output.command_buffer);
}

void ow_end_copy_pass(wasm_exec_env_t exec_env) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);

    DEBUG_CHECK(state->output.render_pass == NULL, "called ow_end_copy_pass when render pass is active");
    DEBUG_CHECK(state->output.copy_pass != NULL, "called ow_end_copy_pass when no pass is active");

    SDL_EndGPUCopyPass(state->output.copy_pass);
    state->output.copy_pass = NULL;
}

void ow_begin_render_pass(wasm_exec_env_t exec_env, uint32_t info_ptr) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);

    DEBUG_CHECK(state->output.copy_pass == NULL, "called ow_begin_render_pass when copy pass is active");
    DEBUG_CHECK(state->output.render_pass == NULL, "called ow_begin_render_pass when render pass is active");

    ow_pass_info* info = (ow_pass_info*)wasm_runtime_addr_app_to_native(instance, info_ptr);

    SDL_GPUColorTargetInfo color_target_info = {0};
    color_target_info.clear_color = (SDL_FColor){.r = info->clear_color_rgba[0],
        .g = info->clear_color_rgba[1],
        .b = info->clear_color_rgba[2],
        .a = info->clear_color_rgba[3]};
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    color_target_info.texture = state->output.swapchain_texture;

    state->output.render_pass = SDL_BeginGPURenderPass(state->output.command_buffer, &color_target_info, 1, NULL);
}

void ow_end_render_pass(wasm_exec_env_t exec_env) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);

    DEBUG_CHECK(state->output.copy_pass == NULL, "called ow_end_render_pass when copy pass is active");
    DEBUG_CHECK(state->output.render_pass != NULL, "called ow_end_render_pass when no pass is active");

    SDL_EndGPURenderPass(state->output.render_pass);
    state->output.render_pass = NULL;
}

uint32_t ow_create_shader_from_file(wasm_exec_env_t exec_env, uint32_t path_ptr, ow_shader_type type) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    const char* path_ptr_real = wasm_runtime_addr_app_to_native(instance, path_ptr);

    uint8_t* code;
    size_t code_size;
    if(!wd_read_from_zip(&state->zip, path_ptr_real, &code, &code_size)) {
        wasm_runtime_set_exception(instance, "");
        return 0;
    }

    SDL_ShaderCross_SPIRV_Info info = {0};
    info.bytecode = code;
    info.bytecode_size = code_size;
    info.entrypoint = "main";
    info.shader_stage =
        (type == OW_SHADER_VERTEX ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

    SDL_ShaderCross_GraphicsShaderMetadata* metadata = SDL_ShaderCross_ReflectGraphicsSPIRV(code, code_size, 0);
    DEBUG_CHECK_RET0(metadata != NULL, "SDL_ShaderCross_ReflectGraphicsSPIRV failed: %s", SDL_GetError());

    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(state->output.gpu, &info, metadata, 0);
    free(code);
    free(metadata);
    DEBUG_CHECK_RET0(shader != NULL, "SDL_ShaderCross_CompileGraphicsShaderFromSPIRV failed: %s", SDL_GetError());

    wd_object_type object_type = (type == OW_SHADER_VERTEX ? WD_OBJECT_VERTEX_SHADER : WD_OBJECT_FRAGMENT_SHADER);
    uint32_t result;
    if(!wd_new_object(&state->object_manager, object_type, shader, &result)) {
        wasm_runtime_set_exception(instance, "");
        return 0;
    }

    return result;
}

uint32_t ow_create_buffer(wasm_exec_env_t exec_env, ow_buffer_type type, uint32_t size) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);

    SDL_GPUBufferCreateInfo info = {0};
    info.usage = (type == OW_BUFFER_VERTEX ? SDL_GPU_BUFFERUSAGE_VERTEX : SDL_GPU_BUFFERUSAGE_INDEX);
    info.size = size;

    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(state->output.gpu, &info);
    DEBUG_CHECK_RET0(buffer != NULL, "SDL_CreateGPUBuffer failed: %s", SDL_GetError());

    uint32_t result;
    if(!wd_new_object(&state->object_manager, WD_OBJECT_BUFFER, buffer, &result)) {
        wasm_runtime_set_exception(instance, "");
        return 0;
    }
    return result;
}

void ow_update_buffer(wasm_exec_env_t exec_env, uint32_t buffer, uint32_t offset, uint32_t data_ptr, uint32_t size) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    void* data_ptr_real = wasm_runtime_addr_app_to_native(instance, data_ptr);
    DEBUG_CHECK(state->output.copy_pass != NULL, "called ow_update_buffer when no copy pass is active");

    SDL_GPUBuffer* buffer_data = NULL;
    wd_object_type object_type;
    wd_get_object(&state->object_manager, buffer, &object_type, (void**)&buffer_data);
    DEBUG_CHECK(buffer_data != NULL, "called ow_update_buffer with non-existent object");
    DEBUG_CHECK(object_type == WD_OBJECT_BUFFER, "called ow_update_buffer with non-buffer object");

    SDL_GPUTransferBufferCreateInfo transfer_info = {0};
    transfer_info.size = size;
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(state->output.gpu, &transfer_info);
    DEBUG_CHECK(transfer_buffer != NULL, "SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());

    void* transfer_data = SDL_MapGPUTransferBuffer(state->output.gpu, transfer_buffer, false);
    DEBUG_CHECK(transfer_data != NULL, "SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());

    memcpy(transfer_data, data_ptr_real, size);
    SDL_UnmapGPUTransferBuffer(state->output.gpu, transfer_buffer);

    SDL_GPUTransferBufferLocation source = {0};
    source.transfer_buffer = transfer_buffer;
    source.offset = 0;

    SDL_GPUBufferRegion dest = {0};
    dest.buffer = buffer_data;
    dest.offset = offset;
    dest.size = size;

    SDL_UploadToGPUBuffer(state->output.copy_pass, &source, &dest, true);
}

uint32_t ow_create_pipeline(wasm_exec_env_t exec_env, uint32_t info_ptr) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    ow_pipeline_info* info = (ow_pipeline_info*)wasm_runtime_addr_app_to_native(instance, info_ptr);
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {0};

    wd_object_type object_type;
    wd_get_object(&state->object_manager, info->vertex_shader, &object_type, (void*)&pipeline_info.vertex_shader);
    DEBUG_CHECK_RET0(
        pipeline_info.vertex_shader != NULL, "vertex_shader object in ow_pipeline_info does not exist or freed");
    DEBUG_CHECK_RET0(
        object_type == WD_OBJECT_VERTEX_SHADER, "vertex_shader object in ow_pipeline_info is not a vertex shader");

    wd_get_object(&state->object_manager, info->fragment_shader, &object_type, (void*)&pipeline_info.fragment_shader);
    DEBUG_CHECK_RET0(
        pipeline_info.fragment_shader != NULL, "fragment_shader object in ow_pipeline_info does not exist or freed");
    DEBUG_CHECK_RET0(object_type == WD_OBJECT_FRAGMENT_SHADER,
        "fragment_shader object in ow_pipeline_info is not a fragment shader");

    SDL_GPUVertexBufferDescription* sdl_vertex_buffer_descriptions =
        calloc(info->vertex_bindings_count, sizeof(SDL_GPUVertexBufferDescription));
    ow_vertex_binding_info* vertex_bindings =
        (ow_vertex_binding_info*)wasm_runtime_addr_app_to_native(instance, info->vertex_bindings_ptr);

    for(uint32_t i = 0; i < info->vertex_bindings_count; i++) {
        sdl_vertex_buffer_descriptions[i].slot = vertex_bindings[i].slot;
        if(vertex_bindings[i].per_instance) {
            sdl_vertex_buffer_descriptions[i].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        } else {
            sdl_vertex_buffer_descriptions[i].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        }
        sdl_vertex_buffer_descriptions[i].pitch = vertex_bindings[i].stride;
    }

    pipeline_info.vertex_input_state.num_vertex_buffers = info->vertex_bindings_count;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = sdl_vertex_buffer_descriptions;

    SDL_GPUVertexAttribute* sdl_vertex_attributes =
        calloc(info->vertex_attributes_count, sizeof(SDL_GPUVertexAttribute));
    ow_vertex_attribute* vertex_attributes =
        (ow_vertex_attribute*)wasm_runtime_addr_app_to_native(instance, info->vertex_attributes_ptr);

    for(uint32_t i = 0; i < info->vertex_attributes_count; i++) {
        sdl_vertex_attributes[i].buffer_slot = vertex_attributes[i].slot;
        sdl_vertex_attributes[i].location = vertex_attributes[i].location;
        sdl_vertex_attributes[i].offset = vertex_attributes[i].offset;

        switch(vertex_attributes[i].type) {
            case OW_ATTRIBUTE_FLOAT3:
                sdl_vertex_attributes[i].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
                break;
            case OW_ATTRIBUTE_FLOAT4:
                sdl_vertex_attributes[i].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
                break;
            default:
                wd_set_scene_error("unknown vertex attribute type %d", vertex_attributes[i].type);
                free(sdl_vertex_buffer_descriptions);
                free(sdl_vertex_attributes);
                wasm_runtime_set_exception(instance, "");
                return 0;
        }
    }

    pipeline_info.vertex_input_state.num_vertex_attributes = info->vertex_attributes_count;
    pipeline_info.vertex_input_state.vertex_attributes = sdl_vertex_attributes;

    switch(info->topology) {
        case OW_TOPOLOGY_TRIANGLES:
            pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            break;
        case OW_TOPOLOGY_TRIANGLES_STRIP:
            pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
            break;
        case OW_TOPOLOGY_LINES:
            pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
            break;
        case OW_TOPOLOGY_LINES_STRIP:
            pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINESTRIP;
            break;
        default:
            wd_set_scene_error("unknown pipeline topology %d", info->topology);
            free(sdl_vertex_attributes);
            free(sdl_vertex_buffer_descriptions);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    SDL_GPUColorTargetDescription color_target_description = {0};
    color_target_description.format = SDL_GetGPUSwapchainTextureFormat(state->output.gpu, state->output.window);
    pipeline_info.target_info.num_color_targets = 1;
    pipeline_info.target_info.color_target_descriptions = &color_target_description;

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(state->output.gpu, &pipeline_info);
    free(sdl_vertex_attributes);
    free(sdl_vertex_buffer_descriptions);
    DEBUG_CHECK_RET0(pipeline != NULL, "SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());

    uint32_t result;
    if(!wd_new_object(&state->object_manager, WD_OBJECT_PIPELINE, pipeline, &result)) {
        return 0;
    }

    return result;
}

void ow_get_screen_size(wasm_exec_env_t exec_env, uint32_t width, uint32_t height) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    uint32_t* width_real = wasm_runtime_addr_app_to_native(instance, width);
    uint32_t* height_real = wasm_runtime_addr_app_to_native(instance, height);

    int width_int, height_int;
    bool ok = SDL_GetWindowSize(state->output.window, &width_int, &height_int);
    DEBUG_CHECK(ok, "SDL_GetWindowSize failed: %s", SDL_GetError());

    *width_real = (uint32_t)width_int;
    *height_real = (uint32_t)height_int;
}

void ow_push_uniform_data(wasm_exec_env_t exec_env, uint32_t type, uint32_t slot, uint32_t data, uint32_t size) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    void* data_real = wasm_runtime_addr_app_to_native(instance, data);
    DEBUG_CHECK(slot < 4, "only 4 uniform data slots are available for one shader type");

    switch(type) {
        case OW_SHADER_VERTEX:
            SDL_PushGPUVertexUniformData(state->output.command_buffer, slot, data_real, size);
            break;
        case OW_SHADER_FRAGMENT:
            SDL_PushGPUFragmentUniformData(state->output.command_buffer, slot, data_real, size);
            break;
        default:
            wd_set_scene_error("unknown shader type %d", type);
            wasm_runtime_set_exception(instance, "");
            return;
    }
}

void ow_render_geometry(wasm_exec_env_t exec_env, uint32_t pipeline, uint32_t bindings_ptr, uint32_t vertex_offset,
    uint32_t vertex_count, uint32_t instance_count) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    DEBUG_CHECK(state->output.render_pass != NULL, "called ow_render_geometry when no render pass is active");

    SDL_GPUGraphicsPipeline* sdl_pipeline = NULL;
    wd_object_type object_type;
    wd_get_object(&state->object_manager, pipeline, &object_type, (void**)&sdl_pipeline);
    DEBUG_CHECK(sdl_pipeline != NULL, "passed non-existent object as ow_render_geometry pipeline");
    DEBUG_CHECK(object_type == WD_OBJECT_PIPELINE, "passed non-pipeline object as ow_render_geometry pipeline");

    ow_bindings_info* bindings = (ow_bindings_info*)wasm_runtime_addr_app_to_native(instance, bindings_ptr);
    SDL_GPUBufferBinding* sdl_vertex_buffer_bindings =
        calloc(bindings->vertex_buffer_bindings_count, sizeof(SDL_GPUBufferBinding));
    ow_vertex_buffer_binding* vertex_buffer_bindings =
        (ow_vertex_buffer_binding*)wasm_runtime_addr_app_to_native(instance, bindings->vertex_buffer_bindings_ptr);

    for(uint32_t i = 0; i < bindings->vertex_buffer_bindings_count; i++) {
        SDL_GPUBuffer* sdl_buffer = NULL;
        wd_get_object(&state->object_manager, vertex_buffer_bindings[i].buffer, &object_type, (void**)&sdl_buffer);
        DEBUG_CHECK(sdl_buffer != NULL, "passed non-existent object as ow_render_geometry vertex buffer");
        DEBUG_CHECK(object_type == WD_OBJECT_BUFFER, "passed non-buffer object as ow_render_geometry vertex buffer");
        sdl_vertex_buffer_bindings[i].buffer = sdl_buffer;
        sdl_vertex_buffer_bindings[i].offset = vertex_buffer_bindings[i].offset;
    }

    SDL_BindGPUGraphicsPipeline(state->output.render_pass, sdl_pipeline);
    SDL_BindGPUVertexBuffers(
        state->output.render_pass, 0, sdl_vertex_buffer_bindings, bindings->vertex_buffer_bindings_count);

    SDL_DrawGPUPrimitives(state->output.render_pass, vertex_count, instance_count, vertex_offset, 0);

    free(sdl_vertex_buffer_bindings);
}
