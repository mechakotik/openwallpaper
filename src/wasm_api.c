#include "wasm_api.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
    color_target_info.load_op = (info->clear_color ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD);
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;

    if(info->color_target == 0) {
        color_target_info.texture = state->output.swapchain_texture;
    } else {
        SDL_GPUTexture* texture = NULL;
        wd_object_type object_type;
        wd_get_object(&state->object_manager, info->color_target, &object_type, (void**)&texture);
        DEBUG_CHECK(texture != NULL, "passed non-existent object as ow_pass_info color target");
        DEBUG_CHECK(object_type == WD_OBJECT_TEXTURE, "passed non-texture object as ow_pass_info color target");
        color_target_info.texture = texture;
    }

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

static uint32_t create_shader_from_bytecode(
    wasm_exec_env_t exec_env, const uint8_t* bytecode, size_t size, ow_shader_type type) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);

    SDL_ShaderCross_SPIRV_Info info = {0};
    info.bytecode = bytecode;
    info.bytecode_size = size;
    info.entrypoint = "main";
    info.shader_stage =
        (type == OW_SHADER_VERTEX ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

    SDL_ShaderCross_GraphicsShaderMetadata* metadata = SDL_ShaderCross_ReflectGraphicsSPIRV(bytecode, size, 0);
    DEBUG_CHECK_RET0(metadata != NULL, "SDL_ShaderCross_ReflectGraphicsSPIRV failed: %s", SDL_GetError());

    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(state->output.gpu, &info, metadata, 0);
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

uint32_t ow_create_shader_from_bytecode(
    wasm_exec_env_t exec_env, uint32_t bytecode_ptr, uint32_t size, ow_shader_type type) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    const uint8_t* bytecode_ptr_real = wasm_runtime_addr_app_to_native(instance, bytecode_ptr);
    return create_shader_from_bytecode(exec_env, bytecode_ptr_real, size, type);
}

uint32_t ow_create_shader_from_file(wasm_exec_env_t exec_env, uint32_t path_ptr, ow_shader_type type) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    const char* path_ptr_real = wasm_runtime_addr_app_to_native(instance, path_ptr);

    uint8_t* bytecode;
    size_t size;
    if(!wd_read_from_zip(&state->zip, path_ptr_real, &bytecode, &size)) {
        wasm_runtime_set_exception(instance, "");
        return 0;
    }

    uint32_t result = create_shader_from_bytecode(exec_env, bytecode, size, type);
    free(bytecode);
    return result;
}

uint32_t ow_create_buffer(wasm_exec_env_t exec_env, ow_buffer_type type, uint32_t size) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);

    wd_object_type object_type;
    switch(type) {
        case OW_BUFFER_VERTEX:
            object_type = WD_OBJECT_VERTEX_BUFFER;
            break;
        case OW_BUFFER_INDEX16:
            object_type = WD_OBJECT_INDEX16_BUFFER;
            break;
        case OW_BUFFER_INDEX32:
            object_type = WD_OBJECT_INDEX32_BUFFER;
            break;
        default:
            wd_set_scene_error("unknown buffer type %d", type);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    SDL_GPUBufferCreateInfo info = {0};
    info.usage = (type == OW_BUFFER_VERTEX ? SDL_GPU_BUFFERUSAGE_VERTEX : SDL_GPU_BUFFERUSAGE_INDEX);
    info.size = size;

    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(state->output.gpu, &info);
    DEBUG_CHECK_RET0(buffer != NULL, "SDL_CreateGPUBuffer failed: %s", SDL_GetError());

    uint32_t result;
    if(!wd_new_object(&state->object_manager, object_type, buffer, &result)) {
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
    DEBUG_CHECK(object_type == WD_OBJECT_VERTEX_BUFFER || object_type == WD_OBJECT_INDEX16_BUFFER ||
                    object_type == WD_OBJECT_INDEX32_BUFFER,
        "called ow_update_buffer with non-buffer object");

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
    SDL_ReleaseGPUTransferBuffer(state->output.gpu, transfer_buffer);
}

uint32_t ow_create_texture(wasm_exec_env_t exec_env, uint32_t info_ptr) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    ow_texture_info* info = (ow_texture_info*)wasm_runtime_addr_app_to_native(instance, info_ptr);

    SDL_GPUTextureCreateInfo texture_info = {0};
    texture_info.width = info->width;
    texture_info.height = info->height;
    texture_info.num_levels = info->mip_levels;
    texture_info.layer_count_or_depth = 1;

    switch(info->format) {
        case OW_TEXTURE_SWAPCHAIN:
            wd_set_scene_error("passed swapchain format as offscreen texture format");
            wasm_runtime_set_exception(instance, "");
            return 0;
        case OW_TEXTURE_RGBA8_UNORM:
            texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            break;
        case OW_TEXTURE_RGBA8_UNORM_SRGB:
            texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
            break;
        case OW_TEXTURE_RGBA16_FLOAT:
            texture_info.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
            break;
        case OW_TEXTURE_R8_UNORM:
            texture_info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
            break;
        case OW_TEXTURE_DEPTH16_UNORM:
            texture_info.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            break;
        default:
            wd_set_scene_error("unknown texture format %d", info->format);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    switch(info->samples) {
        case 0:
            texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
            break;
        case 1:
            texture_info.sample_count = SDL_GPU_SAMPLECOUNT_2;
            break;
        case 2:
            texture_info.sample_count = SDL_GPU_SAMPLECOUNT_4;
            break;
        case 3:
            texture_info.sample_count = SDL_GPU_SAMPLECOUNT_8;
            break;
        default:
            wd_set_scene_error("unsupported texture sample count %d", info->samples);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if(info->render_target) {
        if(info->format == OW_TEXTURE_DEPTH16_UNORM) {
            texture_info.usage |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        } else {
            texture_info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        }
    }

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(state->output.gpu, &texture_info);
    DEBUG_CHECK_RET0(texture != NULL, "SDL_CreateGPUTexture failed: %s", SDL_GetError());

    uint32_t result;
    if(!wd_new_object(&state->object_manager, WD_OBJECT_TEXTURE, texture, &result)) {
        wasm_runtime_set_exception(instance, "");
        return 0;
    }

    return result;
}

uint32_t ow_create_texture_from_png(wasm_exec_env_t exec_env, uint32_t path_ptr, uint32_t info_ptr) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    const char* path = wasm_runtime_addr_app_to_native(instance, path_ptr);
    ow_texture_info* info = (ow_texture_info*)wasm_runtime_addr_app_to_native(instance, info_ptr);
    DEBUG_CHECK_RET0(state->output.copy_pass != NULL, "called ow_create_texture_from_png when no copy pass is active");

    if(info->format != OW_TEXTURE_RGBA8_UNORM && info->format != OW_TEXTURE_RGBA8_UNORM_SRGB) {
        wd_set_scene_error("unsupported texture format (TODO)");
        wasm_runtime_set_exception(instance, "");
        return 0;
    }

    size_t image_size;
    uint8_t* image_data;
    if(!wd_read_from_zip(&state->zip, path, &image_data, &image_size)) {
        free(image_data);
        wasm_runtime_set_exception(instance, "");
        return 0;
    }

    SDL_IOStream* image_stream = SDL_IOFromConstMem(image_data, image_size);
    SDL_Surface* raw_surface = IMG_Load_IO(image_stream, true);
    DEBUG_CHECK_RET0(raw_surface != NULL, "IMG_Load failed: %s", SDL_GetError());

    SDL_Surface* surface = SDL_ConvertSurface(raw_surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(raw_surface);
    DEBUG_CHECK_RET0(surface != NULL, "SDL_ConvertSurface failed: %s", SDL_GetError());
    DEBUG_CHECK_RET0(
        surface->pitch == surface->w * 4, "converted surface pitch is not equal to width * 4, this is not supported");

    info->width = surface->w;
    info->height = surface->h;

    uint32_t result = ow_create_texture(exec_env, info_ptr);
    if(result == 0) {
        SDL_DestroySurface(surface);
        free(image_data);
        return 0;
    }

    SDL_GPUTexture* sdl_texture = NULL;
    wd_object_type object_type;
    wd_get_object(&state->object_manager, result, &object_type, (void**)(&sdl_texture));
    DEBUG_CHECK_RET0(sdl_texture != NULL, "ow_create_texture succeeded, but object is null, please report this");
    DEBUG_CHECK_RET0(object_type == WD_OBJECT_TEXTURE,
        "ow_create_texture succeeded, but object is not a texture, please report this");

    SDL_GPUTransferBufferCreateInfo transfer_info = {0};
    transfer_info.size = surface->w * surface->h * 4;
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(state->output.gpu, &transfer_info);
    DEBUG_CHECK_RET0(transfer_buffer != NULL, "SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());

    void* transfer_data = SDL_MapGPUTransferBuffer(state->output.gpu, transfer_buffer, false);
    DEBUG_CHECK_RET0(transfer_data != NULL, "SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());

    memcpy(transfer_data, surface->pixels, (uint32_t)(surface->w * surface->h * 4));
    SDL_UnmapGPUTransferBuffer(state->output.gpu, transfer_buffer);

    SDL_GPUTextureTransferInfo source = {0};
    source.transfer_buffer = transfer_buffer;
    source.pixels_per_row = surface->w;

    SDL_GPUTextureRegion dest = {0};
    dest.texture = sdl_texture;
    dest.w = surface->w;
    dest.h = surface->h;
    dest.d = 1;

    SDL_UploadToGPUTexture(state->output.copy_pass, &source, &dest, false);
    SDL_ReleaseGPUTransferBuffer(state->output.gpu, transfer_buffer);
    SDL_DestroySurface(surface);
    free(image_data);
    return result;
}

uint32_t ow_create_sampler(wasm_exec_env_t exec_env, uint32_t info_ptr) {
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    wd_state* state = wasm_runtime_get_custom_data(instance);
    ow_sampler_info* info = (ow_sampler_info*)wasm_runtime_addr_app_to_native(instance, info_ptr);

    SDL_GPUSamplerCreateInfo sampler_info = {0};
    sampler_info.min_filter = (info->min_filter == OW_FILTER_LINEAR ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST);
    sampler_info.mag_filter = (info->mag_filter == OW_FILTER_LINEAR ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST);
    sampler_info.mipmap_mode =
        (info->mip_filter == OW_FILTER_LINEAR ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST);

    switch(info->wrap_x) {
        case OW_WRAP_CLAMP:
            sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            break;
        case OW_WRAP_REPEAT:
            sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            break;
        case OW_WRAP_MIRROR:
            sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            break;
        default:
            wd_set_scene_error("unknown wrap mode %d", info->wrap_x);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    switch(info->wrap_y) {
        case OW_WRAP_CLAMP:
            sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            break;
        case OW_WRAP_REPEAT:
            sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            break;
        case OW_WRAP_MIRROR:
            sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            break;
        default:
            wd_set_scene_error("unknown wrap mode %d", info->wrap_y);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    if(info->anisotropy != 0) {
        sampler_info.enable_anisotropy = true;
        sampler_info.max_anisotropy = info->anisotropy;
    }

    SDL_GPUSampler* sampler = SDL_CreateGPUSampler(state->output.gpu, &sampler_info);
    DEBUG_CHECK_RET0(sampler != NULL, "SDL_CreateGPUSampler failed: %s", SDL_GetError());

    uint32_t result;
    if(!wd_new_object(&state->object_manager, WD_OBJECT_SAMPLER, sampler, &result)) {
        wasm_runtime_set_exception(instance, "");
        return 0;
    }

    return result;
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

        if(0 <= vertex_attributes[i].type && vertex_attributes[i].type <= OW_ATTRIBUTE_HALF4) {
            static_assert(OW_ATTRIBUTE_HALF4 + 1 == SDL_GPU_VERTEXELEMENTFORMAT_HALF4, "invalid attribute mapping");
            sdl_vertex_attributes[i].format = vertex_attributes[i].type + 1;
        } else {
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
    switch(info->color_target_format) {
        case OW_TEXTURE_SWAPCHAIN:
            color_target_description.format = SDL_GetGPUSwapchainTextureFormat(state->output.gpu, state->output.window);
            break;
        case OW_TEXTURE_RGBA8_UNORM:
            color_target_description.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            break;
        case OW_TEXTURE_RGBA8_UNORM_SRGB:
            color_target_description.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
            break;
        case OW_TEXTURE_RGBA16_FLOAT:
            color_target_description.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
            break;
        case OW_TEXTURE_R8_UNORM:
            color_target_description.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
            break;
        case OW_TEXTURE_DEPTH16_UNORM:
            wd_set_scene_error("passed depth format as color target format");
            wasm_runtime_set_exception(instance, "");
            break;
        default:
            wd_set_scene_error("unknown color target format %d", info->color_target_format);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    switch(info->blend_mode) {
        case OW_BLEND_NONE:
            break;
        case OW_BLEND_ALPHA:
            color_target_description.blend_state.enable_blend = true;
            color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case OW_BLEND_ALPHA_PREMULTIPLIED:
            color_target_description.blend_state.enable_blend = true;
            color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case OW_BLEND_ADD:
            color_target_description.blend_state.enable_blend = true;
            color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            break;
        case OW_BLEND_ADD_PREMULTIPLIED:
            color_target_description.blend_state.enable_blend = true;
            color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            break;
        case OW_BLEND_MODULATE:
            color_target_description.blend_state.enable_blend = true;
            color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
            color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            break;
        case OW_BLEND_MULTIPLY:
            color_target_description.blend_state.enable_blend = true;
            color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
            color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            break;
        default:
            wd_set_scene_error("unknown blend mode %d", info->blend_mode);
            wasm_runtime_set_exception(instance, "");
            return 0;
    }

    pipeline_info.target_info.num_color_targets = 1;
    pipeline_info.target_info.color_target_descriptions = &color_target_description;

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(state->output.gpu, &pipeline_info);
    free(sdl_vertex_attributes);
    free(sdl_vertex_buffer_descriptions);
    DEBUG_CHECK_RET0(pipeline != NULL, "SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());

    uint32_t result;
    if(!wd_new_object(&state->object_manager, WD_OBJECT_PIPELINE, pipeline, &result)) {
        wasm_runtime_set_exception(instance, "");
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
    bool ok = SDL_GetWindowSizeInPixels(state->output.window, &width_int, &height_int);
    DEBUG_CHECK(ok, "SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());

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
        DEBUG_CHECK(object_type == WD_OBJECT_VERTEX_BUFFER,
            "passed non-vertex buffer object as ow_render_geometry vertex buffer");
        sdl_vertex_buffer_bindings[i].buffer = sdl_buffer;
        sdl_vertex_buffer_bindings[i].offset = vertex_buffer_bindings[i].offset;
    }

    ow_texture_binding* texture_bindings =
        (ow_texture_binding*)wasm_runtime_addr_app_to_native(instance, bindings->texture_bindings_ptr);
    SDL_GPUTextureSamplerBinding* sdl_texture_bindings =
        calloc(bindings->texture_bindings_count, sizeof(SDL_GPUTextureSamplerBinding));

    for(uint32_t i = 0; i < bindings->texture_bindings_count; i++) {
        SDL_GPUTexture* sdl_texture = NULL;
        wd_get_object(&state->object_manager, texture_bindings[i].texture, &object_type, (void**)&sdl_texture);
        DEBUG_CHECK(sdl_texture != NULL, "passed non-existent object as ow_render_geometry texture");
        DEBUG_CHECK(object_type == WD_OBJECT_TEXTURE, "passed non-texture object as ow_render_geometry texture");
        sdl_texture_bindings[i].texture = sdl_texture;

        SDL_GPUSampler* sdl_sampler = NULL;
        wd_get_object(&state->object_manager, texture_bindings[i].sampler, &object_type, (void**)&sdl_sampler);
        DEBUG_CHECK(sdl_sampler != NULL, "passed non-existent object as ow_render_geometry sampler");
        DEBUG_CHECK(object_type == WD_OBJECT_SAMPLER, "passed non-sampler object as ow_render_geometry sampler");
        sdl_texture_bindings[i].sampler = sdl_sampler;
    }

    SDL_BindGPUGraphicsPipeline(state->output.render_pass, sdl_pipeline);
    SDL_BindGPUVertexBuffers(
        state->output.render_pass, 0, sdl_vertex_buffer_bindings, bindings->vertex_buffer_bindings_count);
    SDL_BindGPUFragmentSamplers(state->output.render_pass, 0, sdl_texture_bindings, bindings->texture_bindings_count);

    SDL_DrawGPUPrimitives(state->output.render_pass, vertex_count, instance_count, vertex_offset, 0);

    free(sdl_vertex_buffer_bindings);
    free(sdl_texture_bindings);
}

void ow_render_geometry_indexed(wasm_exec_env_t exec_env, uint32_t pipeline, uint32_t bindings_ptr,
    uint32_t index_offset, uint32_t index_count, uint32_t vertex_offset, uint32_t instance_count) {
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
        DEBUG_CHECK(object_type == WD_OBJECT_VERTEX_BUFFER,
            "passed non-vertex buffer object as ow_render_geometry vertex buffer");
        sdl_vertex_buffer_bindings[i].buffer = sdl_buffer;
        sdl_vertex_buffer_bindings[i].offset = vertex_buffer_bindings[i].offset;
    }

    ow_texture_binding* texture_bindings =
        (ow_texture_binding*)wasm_runtime_addr_app_to_native(instance, bindings->texture_bindings_ptr);
    SDL_GPUTextureSamplerBinding* sdl_texture_bindings =
        calloc(bindings->texture_bindings_count, sizeof(SDL_GPUTextureSamplerBinding));

    for(uint32_t i = 0; i < bindings->texture_bindings_count; i++) {
        SDL_GPUTexture* sdl_texture = NULL;
        wd_get_object(&state->object_manager, texture_bindings[i].texture, &object_type, (void**)&sdl_texture);
        DEBUG_CHECK(sdl_texture != NULL, "passed non-existent object as ow_render_geometry texture");
        DEBUG_CHECK(object_type == WD_OBJECT_TEXTURE, "passed non-texture object as ow_render_geometry texture");
        sdl_texture_bindings[i].texture = sdl_texture;

        SDL_GPUSampler* sdl_sampler = NULL;
        wd_get_object(&state->object_manager, texture_bindings[i].sampler, &object_type, (void**)&sdl_sampler);
        DEBUG_CHECK(sdl_sampler != NULL, "passed non-existent object as ow_render_geometry sampler");
        DEBUG_CHECK(object_type == WD_OBJECT_SAMPLER, "passed non-sampler object as ow_render_geometry sampler");
        sdl_texture_bindings[i].sampler = sdl_sampler;
    }

    SDL_GPUBuffer* sdl_index_buffer = NULL;
    wd_get_object(&state->object_manager, bindings->index_buffer, &object_type, (void**)&sdl_index_buffer);
    DEBUG_CHECK(sdl_index_buffer != NULL, "passed non-existent object as ow_render_geometry index buffer");
    DEBUG_CHECK(object_type == WD_OBJECT_INDEX16_BUFFER || object_type == WD_OBJECT_INDEX32_BUFFER,
        "passed non-index buffer object as ow_render_geometry index buffer");
    SDL_GPUIndexElementSize sdl_index_element_size =
        (object_type == WD_OBJECT_INDEX16_BUFFER ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_GPUBufferBinding sdl_index_buffer_binding = {0};
    sdl_index_buffer_binding.buffer = sdl_index_buffer;
    sdl_index_buffer_binding.offset = 0;

    SDL_BindGPUGraphicsPipeline(state->output.render_pass, sdl_pipeline);
    SDL_BindGPUVertexBuffers(
        state->output.render_pass, 0, sdl_vertex_buffer_bindings, bindings->vertex_buffer_bindings_count);
    SDL_BindGPUIndexBuffer(state->output.render_pass, &sdl_index_buffer_binding, sdl_index_element_size);
    SDL_BindGPUFragmentSamplers(state->output.render_pass, 0, sdl_texture_bindings, bindings->texture_bindings_count);

    SDL_DrawGPUIndexedPrimitives(
        state->output.render_pass, index_count, instance_count, index_offset, vertex_offset, 0);

    free(sdl_vertex_buffer_bindings);
    free(sdl_texture_bindings);
}
