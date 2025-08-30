#ifndef OPENWALLPAPER_H
#define OPENWALLPAPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OW_ID_SCREEN_TARGET 0xFFFFFFFF

typedef uint32_t ow_id;

typedef enum {
    OW_TEXTURE_RGBA8,
    OW_TEXTURE_RGBA8_SRGB,
    OW_TEXTURE_RGBA16F,
    OW_TEXTURE_DEPTH16,
    OW_TEXTURE_DEPTH24_STENCIL8,
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
    OW_BUFFER_INDEX,
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
    OW_BLEND_ADD,
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

typedef struct {
    ow_id color_target;
    bool clear_color;
    float clear_color_rgba[4];
    ow_id depth_target;
    bool clear_depth;
    float clear_depth_value;
    bool clear_stencil;
    uint8_t clear_stencil_value;
} ow_pass_info;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t mip_levels;
    ow_texture_format format;
    bool render_target;
} ow_texture_info;

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
    size_t stride;
    bool per_instance;
} ow_vertex_binding_info;

typedef struct {
    uint32_t location;
    ow_attribute_type type;
    uint32_t slot;
    size_t offset;
} ow_vertex_attribute;

typedef struct {
    const ow_vertex_binding_info* vertex_bindings;
    uint32_t vertex_bindings_count;
    const ow_vertex_attribute* vertex_attributes;
    uint32_t vertex_attributes_count;
    ow_id vertex_shader;
    ow_id fragment_shader;
    ow_blend_mode blend_mode;
    ow_depth_test_mode depth_test_mode;
    bool depth_write;
    ow_topology topology;
    ow_cull_mode cull_mode;
    size_t push_constants_size;
} ow_pipeline_info;

typedef struct {
    uint32_t slot;
    ow_id texture;
    ow_id sampler;
} ow_texture_binding;

typedef struct {
    ow_id buffer;
    size_t offset;
} ow_vertex_buffer_binding;

typedef struct {
    const ow_vertex_buffer_binding* vertex_buffer_bindings;
    uint32_t vertex_buffer_bindings_count;
    ow_id index_buffer;
    size_t index_offset;
    uint32_t index_count;
    const ow_texture_binding* texture_bindings;
    uint32_t texture_bindings_count;
} ow_bindings_info;

void init();
void update(float delta);

extern void ow_log(const char* message);
extern const char* ow_get_last_error();

extern void ow_begin_copy_pass();
extern void ow_end_copy_pass();
extern void ow_begin_render_pass(const ow_pass_info* info);
extern void ow_end_render_pass();

extern ow_id ow_create_buffer(ow_buffer_type type, uint32_t size);
extern void ow_update_buffer(ow_id buffer, uint32_t offset, const void* data, uint32_t size);

extern ow_id ow_create_texture(const void* data, const ow_texture_info* info);
extern ow_id ow_load_texture(const char* path);
extern ow_id ow_create_shader(const char* source, ow_shader_type type);
extern ow_id ow_load_shader(const char* path, ow_shader_type type);
extern ow_id ow_create_sampler(const ow_sampler_info* info);
extern ow_id ow_create_pipeline(const ow_pipeline_info* info);

extern void ow_push_uniform_data(ow_shader_type type, uint32_t slot, const void* data, uint32_t size);
extern void ow_render_geometry(ow_id pipeline, const ow_bindings_info* bindings, uint32_t vertex_offset,
    uint32_t vertex_count, uint32_t instance_count);
extern void ow_render_geometry_indexed(ow_id pipeline, const ow_bindings_info* bindings, uint32_t index_offset,
    uint32_t index_count, uint32_t vertex_offset, uint32_t instance_count);

extern void ow_free(ow_id id);

#endif
