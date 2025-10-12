#ifndef OPENWALLPAPER_H
#define OPENWALLPAPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * A type representing an object ID. It is basically wrapper around a pointer to object stored in host application
 * memory. Valid object ID is never zero, this value is reserved for different purposes.
 *
 * \see `ow_free`
 */
typedef uint32_t ow_id;

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

/**
 * A structure specifying render pass parameters.
 */
typedef struct {
    ow_id color_target;        // ID of color target texture. Setting it to `0` means render target is screen
    bool clear_color;          // If `true`, render pass will clear color target with `clear_color_rgba`
    float clear_color_rgba[4]; // RGBA color to clear color target with
    ow_id depth_target;        // ID of depth target texture
    bool clear_depth;          // If `true`, render pass will clear depth target with `clear_depth_value`
    float clear_depth_value;   // Value to clear depth target with
} ow_pass_info;

/**
 * A structure specifying texture parameters.
 */
typedef struct {
    uint32_t width;      // Width of texture in pixels
    uint32_t height;     // Height of texture in pixels
    uint32_t mip_levels; // Number of mip levels
    uint32_t samples;    // Power of two of number of MSAA samples, e.g. `samples = 3` means 8x MSAA. Clamped to the
                         // maximum supported value
    ow_texture_format format; // Pixel format of texture
    bool render_target;       // If `true`, texture can be used as render target
} ow_texture_info;

/**
 * A structure specifying rectangular texture fragment to update in `ow_update_texture`.
 */
typedef struct {
    ow_id texture;      // ID of texture to update
    uint32_t mip_level; // Mip level to update, must be less that texture's `mip_levels`
    uint32_t x;         // Left offset of destination rectangle
    uint32_t y;         // Top offset of destination rectangle
    uint32_t w;         // Width of destination rectangle
    uint32_t h;         // Height of destination rectangle
} ow_texture_update_destination;

/**
 * A structure specifying sampler parameters.
 */
typedef struct {
    ow_filter_mode min_filter; // Minification filter
    ow_filter_mode mag_filter; // Magnification filter
    ow_filter_mode mip_filter; // Mipmap filter
    ow_wrap_mode wrap_x;       // Wrap mode for X axis
    ow_wrap_mode wrap_y;       // Wrap mode for Y axis
    uint32_t anisotropy;       // Anisotropy level, clamped to the maximum supported value
} ow_sampler_info;

/**
 * A structure specifying vertex buffer binding info.
 */
typedef struct {
    uint32_t slot; // The binding slot of the vertex buffer
    size_t
        stride; // The stride of the vertex buffer in bytes (the size of a single element + the offset between elements)
    bool per_instance; // If `true`, this buffer is used per instance, otherwise it is bound per vertex
} ow_vertex_binding_info;

/**
 * A structure specifying vertex attribute.
 */
typedef struct {
    uint32_t location;      // The location of the attribute in vertex shader
    ow_attribute_type type; // The type of the attribute
    uint32_t slot;          // The binding slot of the associated vertex buffer
    size_t offset;          // The offset of the attribute in bytes from the start of the vertex element
} ow_vertex_attribute;

/**
 * A structure specifying pipeline parameters.
 */
typedef struct {
    const ow_vertex_binding_info* vertex_bindings; // A pointer to an array of vertex buffer bindings
    uint32_t vertex_bindings_count;                // The number of vertex buffer bindings in the array
    const ow_vertex_attribute* vertex_attributes;  // A pointer to an array of vertex attributes
    uint32_t vertex_attributes_count;              // The number of vertex attributes in the array
    ow_texture_format color_target_format;         // The pixel format of the color target texture
    ow_id vertex_shader;                           // ID of vertex shader to use
    ow_id fragment_shader;                         // ID of fragment shader to use
    ow_blend_mode blend_mode;                      // The blend mode to use
    ow_depth_test_mode depth_test_mode;            // The depth test mode to use
    bool depth_write;                              // If `true`, depth test will update the depth target texture
    ow_topology topology;                          // The vertex topology to use
    ow_cull_mode cull_mode;                        // The cull mode to use
} ow_pipeline_info;

/**
 * A structure specifying a texture binding.
 */
typedef struct {
    uint32_t slot; // The slot of the texture in shader
    ow_id texture; // ID of the texture to bind
    ow_id sampler; // ID of the sampler to use for this texture
} ow_texture_binding;

/**
 * A structure specifying bindings for draw call.
 */
typedef struct {
    const ow_id* vertex_buffers;                // A pointer to an array of vertex buffer IDs
    uint32_t vertex_buffers_count;              // The number of vertex buffer IDs in the array
    ow_id index_buffer;                         // ID of the index buffer
    const ow_texture_binding* texture_bindings; // A pointer to an array of texture bindings
    uint32_t texture_bindings_count;            // The number of texture bindings in the array
} ow_bindings_info;

/**
 * A user-implemented function that is called once when scene is initializing.
 */
void init();

/**
 * A user-implemented function that is called each frame and should update/redraw the scene.
 *
 * \param delta Time elapsed since the last frame in seconds. You should multiply all the scene movements by this value
 * in order to make your scene framerate independent
 */
void update(float delta);

/**
 * Prints a message to the console.
 *
 * \param message A null-terminated byte string to print
 */
extern void ow_log(const char* message);

/**
 * Loads a file from the scene archive into module memory. Crashes scene if file is not found.
 *
 * \param path Path to the file to load, absolute in the scene archive. A null-terminated byte string
 */
extern void ow_load_file(const char* path, uint8_t** data, size_t* size);

/**
 * Begins a copy pass. Can be called only if no pass is currently active.
 */
extern void ow_begin_copy_pass();
extern void ow_end_copy_pass();
extern void ow_begin_render_pass(const ow_pass_info* info);
extern void ow_end_render_pass();

extern ow_id ow_create_buffer(ow_buffer_type type, uint32_t size);
extern void ow_update_buffer(ow_id buffer, uint32_t offset, const void* data, uint32_t size);
extern ow_id ow_create_texture(const ow_texture_info* info);
extern ow_id ow_create_texture_from_png(const char* path, const ow_texture_info* info);
extern void ow_update_texture(const void* data, uint32_t pixels_per_row, const ow_texture_update_destination* dest);
extern ow_id ow_generate_mipmaps(ow_id texture);
extern ow_id ow_create_sampler(const ow_sampler_info* info);
extern ow_id ow_create_shader_from_bytecode(const uint8_t* bytecode, size_t size, ow_shader_type type);
extern ow_id ow_create_shader_from_file(const char* path, ow_shader_type type);
extern ow_id ow_create_pipeline(const ow_pipeline_info* info);

extern void ow_get_screen_size(uint32_t* width, uint32_t* height);
extern void ow_push_uniform_data(ow_shader_type type, uint32_t slot, const void* data, uint32_t size);
extern void ow_render_geometry(ow_id pipeline, const ow_bindings_info* bindings, uint32_t vertex_offset,
    uint32_t vertex_count, uint32_t instance_count);
extern void ow_render_geometry_indexed(ow_id pipeline, const ow_bindings_info* bindings, uint32_t index_offset,
    uint32_t index_count, uint32_t vertex_offset, uint32_t instance_count);

extern void ow_free(ow_id id);

#endif
