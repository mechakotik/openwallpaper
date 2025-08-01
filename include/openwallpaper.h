#ifndef OPENWALLPAPER_H
#define OPENWALLPAPER_H

#include <stdbool.h>
#include <stdint.h>

#define OW_ID_INVALID 0
#define OW_ID_SCREEN_TARGET 0xFFFFFFFF

typedef uint32_t ow_id;

typedef enum : uint8_t { OW_SHADER_VERTEX, OW_SHADER_FRAGMENT } ow_shader_type;

typedef enum : uint8_t {
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

typedef enum : uint8_t {
    OW_BLEND_NONE,
    OW_BLEND_ALPHA,
    OW_BLEND_ADD,
} ow_blend_mode;

typedef enum : uint8_t {
    OW_DEPTH_TEST_DISABLED,
    OW_DEPTH_TEST_ALWAYS,
    OW_DEPTH_TEST_LESS,
    OW_DEPTH_TEST_LESS_EQUAL,
    OW_DEPTH_TEST_GREATER,
    OW_DEPTH_TEST_GREATER_EQUAL,
    OW_DEPTH_TEST_EQUAL,
    OW_DEPTH_TEST_NOT_EQUAL,
} ow_depth_test_mode;

void ow_log(const char* message);

ow_id ow_load_texture(const char* path);
ow_id ow_load_shader(const char* path, ow_shader_type type);

ow_id ow_create_vertex_buffer(const void* data, uint32_t size);
ow_id ow_create_index_buffer(const uint32_t* data, uint32_t count);
void ow_update_vertex_buffer(ow_id buffer, const void* data, uint32_t size);

ow_id ow_create_vertex_layout();
void ow_add_vertex_attribute(ow_id layout, uint32_t location, ow_attribute_type type, uint32_t buffer_index,
    uint32_t stride, uint32_t offset, bool per_instance);

ow_id ow_create_pipeline(ow_id vertex_layout, ow_id vertex_shader, ow_id fragment_shader, ow_blend_mode blend_mode,
    ow_depth_test_mode depth_test_mode, bool depth_write);

void ow_push_uniform_data(const void* data, uint32_t size);
void ow_bind_vertex_buffers(const ow_id* vertex_buffers, uint32_t count);
void ow_bind_index_buffer(ow_id index_buffer, uint32_t index_offset, uint32_t index_count);
void ow_bind_textures(const ow_id* textures, uint32_t texture_count);

void ow_set_render_target(ow_id target);
void ow_render_clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ow_render_geometry(ow_id pipeline);
void ow_render_geometry_instanced(ow_id pipeline, uint32_t instance_count);
void ow_render_flush();

void ow_free(ow_id id);

#endif
