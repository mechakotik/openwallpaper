#ifndef OPENWALLPAPER_H
#define OPENWALLPAPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * A handle pointing to vertex buffer stored in host memory. Existing vertex buffer's id is never zero, this
 * value is reserved to act as non-existent object.
 *
 * \see `ow_free_vertex_buffer`
 */

typedef struct {
    uint32_t id;
} ow_vertex_buffer_id;

/**
 * A handle pointing to index buffer stored in host memory. Existing index buffer's id is never zero, this
 * value is reserved to act as non-existent object.
 *
 * \see `ow_free_index_buffer`
 */

typedef struct {
    uint32_t id;
} ow_index_buffer_id;

/**
 * A handle pointing to GPU texture stored in host memory. Existing texture's id is never zero, this value is reserved
 * to act as non-existent object.
 *
 * \see `ow_free_texture`
 */

typedef struct {
    uint32_t id;
} ow_texture_id;

/**
 * A handle pointing to GPU sampler stored in host memory. Existing sampler's id is never zero, this value is reserved
 * to act as non-existent object.
 *
 * \see `ow_free_sampler`
 */

typedef struct {
    uint32_t id;
} ow_sampler_id;

/**
 * A handle pointing to vertex shader stored in host memory. Existing vertex shader's id is never zero, this value is
 * reserved to act as non-existent object.
 *
 * \see `ow_free_vertex_shader`
 */

typedef struct {
    uint32_t id;
} ow_vertex_shader_id;

/**
 * A handle pointing to fragment shader stored in host memory. Existing fragment shader's id is never zero, this value
 * is reserved to act as non-existent object.
 *
 * \see `ow_free_fragment_shader`
 */

typedef struct {
    uint32_t id;
} ow_fragment_shader_id;

/**
 * A handle pointing to GPU pipeline state object stored in host memory. Existing pipeline's id is never zero, this
 * value is reserved to act as non-existent object.
 *
 * \see `ow_free_pipeline`
 */

typedef struct {
    uint32_t id;
} ow_pipeline_id;

typedef enum {
    OW_MSAA_OFF,
    OW_MSAA_2X,
    OW_MSAA_4X,
    OW_MSAA_8X,
} ow_msaa_samples;

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

typedef enum {
    OW_BUTTON_LEFT = (1 << 0),
    OW_BUTTON_RIGHT = (1 << 1),
    OW_BUTTON_MIDDLE = (1 << 2),
    OW_BUTTON_X1 = (1 << 3),
    OW_BUTTON_X2 = (1 << 4),
} ow_mouse_button;

/**
 * A structure specifying render pass parameters.
 */
typedef struct {
    ow_texture_id color_target; ///< ID of color target texture. Setting it to `0` means render target is screen
    bool clear_color;           ///< If `true`, render pass will clear color target with `clear_color_rgba`
    float clear_color_rgba[4];  ///< RGBA color to clear color target with
    ow_texture_id depth_target; ///< ID of depth target texture
    bool clear_depth;           ///< If `true`, render pass will clear depth target with `clear_depth_value`
    float clear_depth_value;    ///< Value to clear depth target with
} ow_pass_info;

/**
 * A structure specifying texture parameters.
 */
typedef struct {
    uint32_t width;           ///< Width of texture in pixels
    uint32_t height;          ///< Height of texture in pixels
    uint32_t mip_levels;      ///< Number of mip levels
    ow_msaa_samples samples;  ///< Number of MSAA samples
    ow_texture_format format; ///< Pixel format of texture
    bool render_target;       ///< If `true`, texture can be used as render target
} ow_texture_info;

/**
 * A structure specifying rectangular texture fragment to update in `ow_update_texture`.
 */
typedef struct {
    ow_texture_id texture; ///< ID of texture to update
    uint32_t mip_level;    ///< Mip level to update, must be less than texture's `mip_levels`
    uint32_t x;            ///< Left offset of destination rectangle
    uint32_t y;            ///< Top offset of destination rectangle
    uint32_t w;            ///< Width of destination rectangle
    uint32_t h;            ///< Height of destination rectangle
} ow_texture_update_destination;

/**
 * A structure specifying sampler parameters.
 */
typedef struct {
    ow_filter_mode min_filter; ///< Minification filter
    ow_filter_mode mag_filter; ///< Magnification filter
    ow_filter_mode mip_filter; ///< Mipmap filter
    ow_wrap_mode wrap_x;       ///< Wrap mode for X axis
    ow_wrap_mode wrap_y;       ///< Wrap mode for Y axis
    uint32_t anisotropy;       ///< Anisotropy level, clamped to the maximum supported value
} ow_sampler_info;

/**
 * A structure specifying vertex buffer binding info.
 */
typedef struct {
    uint32_t slot;     ///< The binding slot of the vertex buffer
    size_t stride;     ///< The stride of the vertex buffer in bytes (the size of a single element + the offset between
                       ///< elements)
    bool per_instance; ///< If `true`, this buffer is used per instance, otherwise it is bound per vertex
} ow_vertex_binding_info;

/**
 * A structure specifying vertex attribute.
 */
typedef struct {
    uint32_t location;      ///< The location of the attribute in vertex shader
    ow_attribute_type type; ///< The type of the attribute
    uint32_t slot;          ///< The binding slot of the associated vertex buffer
    size_t offset;          ///< The offset of the attribute in bytes from the start of the vertex element
} ow_vertex_attribute;

/**
 * A structure specifying pipeline parameters.
 */
typedef struct {
    const ow_vertex_binding_info* vertex_bindings; ///< A pointer to an array of vertex buffer bindings
    uint32_t vertex_bindings_count;                ///< The number of vertex buffer bindings in the array
    const ow_vertex_attribute* vertex_attributes;  ///< A pointer to an array of vertex attributes
    uint32_t vertex_attributes_count;              ///< The number of vertex attributes in the array
    ow_texture_format color_target_format;         ///< The pixel format of the color target texture
    ow_vertex_shader_id vertex_shader;             ///< ID of vertex shader to use
    ow_fragment_shader_id fragment_shader;         ///< ID of fragment shader to use
    ow_blend_mode blend_mode;                      ///< The blend mode to use
    ow_depth_test_mode depth_test_mode;            ///< The depth test mode to use
    bool depth_write;                              ///< If `true`, depth test will update the depth target texture
    ow_topology topology;                          ///< The vertex topology to use
    ow_cull_mode cull_mode;                        ///< The cull mode to use
} ow_pipeline_info;

/**
 * A structure specifying a texture binding.
 */
typedef struct {
    uint32_t slot;         ///< The slot of the texture in shader
    ow_texture_id texture; ///< ID of the texture to bind
    ow_sampler_id sampler; ///< ID of the sampler to use for this texture
} ow_texture_binding;

/**
 * A structure specifying bindings for draw call.
 */
typedef struct {
    const ow_vertex_buffer_id* vertex_buffers;  ///< A pointer to an array of vertex buffer IDs
    uint32_t vertex_buffers_count;              ///< The number of vertex buffer IDs in the array
    ow_index_buffer_id index_buffer;            ///< ID of the index buffer
    const ow_texture_binding* texture_bindings; ///< A pointer to an array of texture bindings
    uint32_t texture_bindings_count;            ///< The number of texture bindings in the array
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
 * Loads a file from the scene archive into module memory. Panics if file is not found. Memory for loaded data is
 * allocated by the host application during function call, and after that owned by user.
 *
 * \param path Path to the file to load, absolute in the scene archive. A null-terminated byte string
 * \param data Loaded data
 * \param size Size of loaded data in bytes
 */
extern void ow_load_file(const char* path, uint8_t** data, size_t* size);

/**
 * Begins a copy pass. Can be called only if no pass is currently active, panics elsewhere.
 */
extern void ow_begin_copy_pass();

/**
 * Ends a copy pass. Can be called only if a copy pass is currently active, panics elsewhere.
 */
extern void ow_end_copy_pass();

/**
 * Begins a render pass. Can be called only if no pass is currently active, panics elsewhere.
 *
 * \param info Render pass parameters
 */
extern void ow_begin_render_pass(const ow_pass_info* info);

/**
 * Ends a render pass. Can be called only if a render pass is currently active, panics elsewhere.
 */
extern void ow_end_render_pass();

/**
 * Creates a vertex buffer of given `size`.
 *
 * \param type Type of the buffer
 * \param size Buffer size in bytes
 * \return ID of created buffer
 */
extern ow_vertex_buffer_id ow_create_vertex_buffer(uint32_t size);

/**
 * Creates an index buffer of given `size`. If `wide` is `true`, the buffer will be created with 32-bit indices,
 * otherwise with 16-bit indices.
 *
 * \param type Type of the buffer
 * \param size Buffer size in bytes
 * \return ID of created buffer
 */
extern ow_index_buffer_id ow_create_index_buffer(uint32_t size, bool wide);

/**
 * Overwrites vertex buffer data subsegment beginning at `offset` with `size` bytes from `data`. Can be called only if
 * copy pass is currently active, panics elsewhere.
 *
 * \param buffer Vertex buffer ID to update
 * \param offset Offset in bytes from the start of the buffer
 * \param data Pointer to the source data
 * \param size Size of subsegment to update in bytes
 */
extern void ow_update_vertex_buffer(ow_vertex_buffer_id buffer, uint32_t offset, const void* data, uint32_t size);

/**
 * Overwrites index buffer data subsegment beginning at `offset` with `size` bytes from `data`. Can be called only if
 * copy pass is currently active, panics elsewhere.
 *
 * \param buffer Index buffer ID to update
 * \param offset Offset in bytes from the start of the buffer
 * \param data Pointer to the source data
 * \param size Size of subsegment to update in bytes
 */
extern void ow_update_index_buffer(ow_index_buffer_id buffer, uint32_t offset, const void* data, uint32_t size);

/**
 * Creates a texture.
 *
 * \param info Texture parameters
 * \return ID of created texture
 */
extern ow_texture_id ow_create_texture(const ow_texture_info* info);

/**
 * Creates a texture from a WEBP file from the scene archive. Panics if file is not found.
 *
 * \param path Path to the file to load, absolute in the scene archive. A null-terminated byte string
 * \param info Texture parameters
 * \return ID of created texture
 */
extern ow_texture_id ow_create_texture_from_webp(const char* path, const ow_texture_info* info);

/**
 * Updates a `dest` texture region with data from `data`
 *
 * \param data Pointer to the source data
 * \param pixels_per_row Number of pixels per row in the source data
 * \param dest Pointer to the destination texture region
 */
extern void ow_update_texture(const void* data, uint32_t pixels_per_row, const ow_texture_update_destination* dest);

/**
 * Generates mipmaps for a texture.
 *
 * \param texture Texture ID to generate mipmaps for.
 */
extern void ow_generate_mipmaps(ow_texture_id texture);

/**
 * Creates a sampler.
 *
 * \param info Sampler parameters
 * \return ID of created sampler
 */
extern ow_sampler_id ow_create_sampler(const ow_sampler_info* info);

/**
 * Creates a vertex shader from SPIR-V bytecode.
 *
 * \param bytecode Pointer to the bytecode
 * \param size Size of the bytecode in bytes
 * \return ID of created vertex shader
 */
extern ow_vertex_shader_id ow_create_vertex_shader_from_bytecode(const uint8_t* bytecode, size_t size);

/**
 * Creates a vertex shader from a SPIR-V bytecode file from the scene archive. Panics if file is not found.
 *
 * \param path Path to the file to load, absolute in the scene archive. A null-terminated byte string
 * \return ID of created vertex shader
 */
extern ow_vertex_shader_id ow_create_vertex_shader_from_file(const char* path);

/**
 * Creates a fragment shader from SPIR-V bytecode.
 *
 * \param bytecode Pointer to the bytecode
 * \param size Size of the bytecode in bytes
 * \return ID of created fragment shader
 */
extern ow_fragment_shader_id ow_create_fragment_shader_from_bytecode(const uint8_t* bytecode, size_t size);

/**
 * Creates a fragment shader from a SPIR-V bytecode file from the scene archive. Panics if file is not found.
 *
 * \param path Path to the file to load, absolute in the scene archive. A null-terminated byte string
 * \return ID of created fragment shader
 */
extern ow_fragment_shader_id ow_create_fragment_shader_from_file(const char* path);

/**
 * Creates a graphics pipeline.
 *
 * \param info Pipeline parameters
 * \return ID of created pipeline
 */
extern ow_pipeline_id ow_create_pipeline(const ow_pipeline_info* info);

/**
 * Pushes vertex shader uniform data for given slot. Subsequent `ow_render_geometry` and `ow_render_geometry_indexed`
 * calls will use this data until overwritten or render pass ends. The pushed data must respect std140 layout
 * conventions. Can be called only if render pass is currently active, panics elsewhere.
 *
 * \param slot Target uniform slot
 * \param data Pointer to the data to push
 * \param size Size of the data in bytes
 */
extern void ow_push_vertex_uniform_data(uint32_t slot, const void* data, uint32_t size);

/**
 * Pushes fragment shader uniform data for given slot. Subsequent `ow_render_geometry` and `ow_render_geometry_indexed`
 * calls will use this data until overwritten or render pass ends. The pushed data must respect std140 layout
 * conventions. Can be called only if render pass is currently active, panics elsewhere.
 *
 * \param slot Target uniform slot
 * \param data Pointer to the data to push
 * \param size Size of the data in bytes
 */
extern void ow_push_fragment_uniform_data(uint32_t slot, const void* data, uint32_t size);

/**
 * Renders geometry primitives.
 *
 * \param pipeline Pipeline ID to use
 * \param bindings Pointer to the bindings info
 * \param vertex_offset Offset in vertices to start rendering from
 * \param vertex_count Number of vertices to render
 * \param instance_count Number of instances
 */
extern void ow_render_geometry(ow_pipeline_id pipeline, const ow_bindings_info* bindings, uint32_t vertex_offset,
    uint32_t vertex_count, uint32_t instance_count);

/**
 * Renders geometry primitives with indices from an index buffer.
 *
 * \param pipeline Pipeline ID to use
 * \param bindings Pointer to the bindings info
 * \param index_offset Offset in indices to start rendering from
 * \param index_count Number of indices to render
 * \param vertex_offset Offset in vertices to start rendering from
 * \param instance_count Number of instances
 */
extern void ow_render_geometry_indexed(ow_pipeline_id pipeline, const ow_bindings_info* bindings, uint32_t index_offset,
    uint32_t index_count, uint32_t vertex_offset, uint32_t instance_count);

/**
 * Gets the screen size in pixels.
 *
 * \param width Pointer to the variable to store the width in pixels
 * \param height Pointer to the variable to store the height in pixels
 */
extern void ow_get_screen_size(uint32_t* width, uint32_t* height);

/**
 * Gets cursor position and mouse buttons state. The cooordinate system for cursor position is x: [0, width]
 * and y: [0, height] where (0, 0) is a top left corner.
 *
 * \param x Pointer to the variable to store the X coordinate in pixels
 * \param y Pointer to the variable to store the Y coordinate in pixels
 * \return Mask with pressed mouse buttons (see enum `ow_mouse_button`)
 */
extern uint32_t ow_get_mouse_state(float* x, float* y);

/**
 * Gets a wallpaper option value by name. Returns `NULL` if wallpaper option with given name is unspecified. The
 * returned string is owned by the host application and must not be freed.
 *
 * \param name Name of the option to get
 * \return Value of the option, a null-terminated byte string
 */
extern const char* ow_get_option(const char* name);

/**
 * Frees a vertex buffer by ID. Panics if vertex buffer is not found or is already freed. Does nothing if `id` is `0`.
 *
 * \param id Vertex buffer ID to free
 * \see ow_create_vertex_buffer
 */
extern void ow_free_vertex_buffer(ow_vertex_buffer_id id);

/**
 * Frees an index buffer by ID. Panics if index buffer is not found or is already freed. Does nothing if `id` is `0`.
 *
 * \param id Index buffer ID to free
 * \see ow_create_index_buffer
 */
extern void ow_free_index_buffer(ow_index_buffer_id id);

/**
 * Frees a texture by ID. Panics if texture is not found or is already freed. Does nothing if `id` is `0`.
 *
 * \param id Texture ID to free
 * \see ow_create_texture
 * \see ow_create_texture_from_webp
 */
extern void ow_free_texture(ow_texture_id id);

/**
 * Frees a sampler by ID. Panics if sampler is not found or is already freed. Does nothing if `id` is `0`.
 *
 * \param id Sampler ID to free
 * \see ow_create_sampler
 */
extern void ow_free_sampler(ow_sampler_id id);

/**
 * Frees a vertex shader by ID. Panics if vertex shader is not found or is already freed. Does nothing if `id` is `0`.
 *
 * \param id Sampler ID to free
 * \see ow_create_sampler
 */
extern void ow_free_vertex_shader(ow_vertex_shader_id id);

/**
 * Frees a fragment shader by ID. Panics if fragment shader is not found or is already freed. Does nothing if `id` is
 * `0`.
 *
 * \param id Sampler ID to free
 * \see ow_create_sampler
 */
extern void ow_free_fragment_shader(ow_fragment_shader_id id);

/**
 * Frees a pipeline ID. Panics if pipeline is not found or is already freed. Does nothing if `id` is `0`.
 *
 * \param id Pipeline ID to free
 * \see ow_create_pipeline
 */
extern void ow_free_pipeline(ow_pipeline_id id);

#endif
