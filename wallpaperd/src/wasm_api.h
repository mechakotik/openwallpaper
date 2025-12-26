#ifndef WASM_API_H
#define WASM_API_H

#include <stdint.h>
#include "wasm_export.h"

#define OW_ID_SCREEN_TARGET 0xFFFFFFFF

typedef enum {
    OW_TEXTURE_SWAPCHAIN,
    OW_TEXTURE_RGBA8_UNORM,
    OW_TEXTURE_RGBA8_UNORM_SRGB,
    OW_TEXTURE_RGBA16_FLOAT,
    OW_TEXTURE_R8_UNORM,
    OW_TEXTURE_DEPTH16_UNORM,
} ow_texture_format;

typedef enum {
    OW_WRAP_CLAMP,
    OW_WRAP_REPEAT,
    OW_WRAP_MIRROR,
} ow_wrap_mode;

typedef enum {
    OW_FILTER_NEAREST,
    OW_FILTER_LINEAR,
} ow_filter_mode;

typedef enum {
    OW_SHADER_VERTEX,
    OW_SHADER_FRAGMENT,
} ow_shader_type;

typedef enum {
    OW_BUFFER_VERTEX,
    OW_BUFFER_INDEX16,
    OW_BUFFER_INDEX32,
} ow_buffer_type;

typedef enum {
    OW_ATTRIBUTE_INT,
    OW_ATTRIBUTE_INT2,
    OW_ATTRIBUTE_INT3,
    OW_ATTRIBUTE_INT4,
    OW_ATTRIBUTE_UINT,
    OW_ATTRIBUTE_UINT2,
    OW_ATTRIBUTE_UINT3,
    OW_ATTRIBUTE_UINT4,
    OW_ATTRIBUTE_FLOAT,
    OW_ATTRIBUTE_FLOAT2,
    OW_ATTRIBUTE_FLOAT3,
    OW_ATTRIBUTE_FLOAT4,
    OW_ATTRIBUTE_BYTE2,
    OW_ATTRIBUTE_BYTE4,
    OW_ATTRIBUTE_UBYTE2,
    OW_ATTRIBUTE_UBYTE4,
    OW_ATTRIBUTE_BYTE2_NORM,
    OW_ATTRIBUTE_BYTE4_NORM,
    OW_ATTRIBUTE_UBYTE2_NORM,
    OW_ATTRIBUTE_UBYTE4_NORM,
    OW_ATTRIBUTE_SHORT2,
    OW_ATTRIBUTE_SHORT4,
    OW_ATTRIBUTE_USHORT2,
    OW_ATTRIBUTE_USHORT4,
    OW_ATTRIBUTE_SHORT2_NORM,
    OW_ATTRIBUTE_SHORT4_NORM,
    OW_ATTRIBUTE_USHORT2_NORM,
    OW_ATTRIBUTE_USHORT4_NORM,
    OW_ATTRIBUTE_HALF2,
    OW_ATTRIBUTE_HALF4,
} ow_attribute_type;

typedef enum {
    OW_BLEND_NONE,
    OW_BLEND_ALPHA,
    OW_BLEND_ALPHA_PREMULTIPLIED,
    OW_BLEND_ADD,
    OW_BLEND_ADD_PREMULTIPLIED,
    OW_BLEND_MODULATE,
    OW_BLEND_MULTIPLY,
} ow_blend_mode;

typedef enum {
    OW_DEPTH_TEST_DISABLED,
    OW_DEPTH_TEST_ALWAYS,
    OW_DEPTH_TEST_LESS,
    OW_DEPTH_TEST_LESS_EQUAL,
    OW_DEPTH_TEST_GREATER,
    OW_DEPTH_TEST_GREATER_EQUAL,
    OW_DEPTH_TEST_EQUAL,
    OW_DEPTH_TEST_NOT_EQUAL,
} ow_depth_test_mode;

typedef enum {
    OW_TOPOLOGY_TRIANGLES,
    OW_TOPOLOGY_TRIANGLES_STRIP,
    OW_TOPOLOGY_LINES,
    OW_TOPOLOGY_LINES_STRIP,
} ow_topology;

typedef enum {
    OW_CULL_NONE,
    OW_CULL_FRONT,
    OW_CULL_BACK,
} ow_cull_mode;

enum {
    OW_BUTTON_LEFT = (1 << 0),
    OW_BUTTON_RIGHT = (1 << 1),
    OW_BUTTON_MIDDLE = (1 << 2),
    OW_BUTTON_X1 = (1 << 3),
    OW_BUTTON_X2 = (1 << 4),
};

typedef struct {
    uint32_t color_target;
    uint32_t clear_color;
    float clear_color_rgba[4];
    uint32_t depth_target;
    uint32_t clear_depth;
    float clear_depth_value;
} ow_pass_info;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t mip_levels;
    uint32_t samples;
    ow_texture_format format;
    uint32_t render_target;
} ow_texture_info;

typedef struct {
    uint32_t texture;
    uint32_t mip_level;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} ow_texture_update_destination;

typedef struct {
    ow_filter_mode min_filter;
    ow_filter_mode mag_filter;
    ow_filter_mode mip_filter;
    ow_wrap_mode wrap_x;
    ow_wrap_mode wrap_y;
    uint32_t anisotropy;
} ow_sampler_info;

typedef struct {
    uint32_t slot;
    uint32_t stride;
    uint32_t per_instance;
} ow_vertex_binding_info;

typedef struct {
    uint32_t location;
    ow_attribute_type type;
    uint32_t slot;
    uint32_t offset;
} ow_vertex_attribute;

typedef struct {
    uint32_t vertex_bindings_ptr;
    uint32_t vertex_bindings_count;
    uint32_t vertex_attributes_ptr;
    uint32_t vertex_attributes_count;
    ow_texture_format color_target_format;
    uint32_t vertex_shader;
    uint32_t fragment_shader;
    ow_blend_mode blend_mode;
    ow_depth_test_mode depth_test_mode;
    uint32_t depth_write;
    ow_topology topology;
    ow_cull_mode cull_mode;
} ow_pipeline_info;

typedef struct {
    uint32_t slot;
    uint32_t texture;
    uint32_t sampler;
} ow_texture_binding;

typedef struct {
    uint32_t vertex_buffers_ptr;
    uint32_t vertex_buffers_count;
    uint32_t index_buffer;
    uint32_t texture_bindings_ptr;
    uint32_t texture_bindings_count;
} ow_bindings_info;

void init();
void update(float delta);

void ow_load_file(wasm_exec_env_t exec_env, uint32_t path_ptr, uint32_t data_ptr, uint32_t size_ptr);

void ow_begin_copy_pass(wasm_exec_env_t exec_env);
void ow_end_copy_pass(wasm_exec_env_t exec_env);
void ow_begin_render_pass(wasm_exec_env_t exec_env, uint32_t info_ptr);
void ow_end_render_pass(wasm_exec_env_t exec_env);

uint32_t ow_create_buffer(wasm_exec_env_t exec_env, ow_buffer_type type, uint32_t size);
void ow_update_buffer(wasm_exec_env_t exec_env, uint32_t buffer, uint32_t offset, uint32_t data_ptr, uint32_t size);
uint32_t ow_create_texture(wasm_exec_env_t exec_env, uint32_t info_ptr);
uint32_t ow_create_texture_from_webp(wasm_exec_env_t exec_env, uint32_t path_ptr, uint32_t info_ptr);
void ow_update_texture(wasm_exec_env_t exec_env, uint32_t data_ptr, uint32_t pixels_per_row, uint32_t dest_ptr);
void ow_generate_mipmaps(wasm_exec_env_t exec_env, uint32_t texture);
uint32_t ow_create_sampler(wasm_exec_env_t exec_env, uint32_t info_ptr);
uint32_t ow_create_shader_from_bytecode(
    wasm_exec_env_t exec_env, uint32_t bytecode_ptr, uint32_t size, ow_shader_type type);
uint32_t ow_create_shader_from_file(wasm_exec_env_t exec_env, uint32_t path_ptr, ow_shader_type type);
uint32_t ow_create_pipeline(wasm_exec_env_t exec_env, uint32_t info_ptr);

void ow_get_screen_size(wasm_exec_env_t exec_env, uint32_t width_ptr, uint32_t height_ptr);
void ow_push_uniform_data(
    wasm_exec_env_t exec_env, ow_shader_type type, uint32_t slot, uint32_t data_ptr, uint32_t size);
void ow_render_geometry(wasm_exec_env_t exec_env, uint32_t pipeline, uint32_t bindings_ptr, uint32_t vertex_offset,
    uint32_t vertex_count, uint32_t instance_count);
void ow_render_geometry_indexed(wasm_exec_env_t exec_env, uint32_t pipeline, uint32_t bindings_ptr,
    uint32_t index_offset, uint32_t index_count, uint32_t vertex_offset, uint32_t instance_count);

uint32_t ow_get_mouse_state(wasm_exec_env_t exec_env, uint32_t x_ptr, uint32_t y_ptr);

void ow_free(wasm_exec_env_t exec_env, uint32_t id);

#endif
