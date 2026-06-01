#ifndef WPE_COMPILE_DEFS_H
#define WPE_COMPILE_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "openwallpaper.h"

typedef enum {
    SCALE_MODE_STRETCH,
    SCALE_MODE_ASPECT_FIT,
    SCALE_MODE_ASPECT_CROP,
} wpe_scale_mode;

typedef union {
    float at[2];
    struct {
        float x, y;
    };
    struct {
        float w, h;
    };
    struct {
        float r, g;
    };
} wpe_vec2;

typedef union {
    float at[3];
    struct {
        float x, y, z;
    };
    struct {
        float r, g, b;
    };
} wpe_vec3;

typedef union {
    float at[4];
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
} wpe_vec4;

typedef struct {
    float at[3][4];
} wpe_mat3;

typedef struct {
    float at[4][4];
} wpe_mat4;

typedef struct {
    float position[3];
    float texcoord[2];
} wpe_vertex_quad_data;

typedef struct wpe_texture wpe_texture;
typedef struct wpe_effect_fbo wpe_effect_fbo;

typedef struct {
    const char* name;
    const char* constant_name;
    const char* type;
    int array_size;
    const float* default_value;
    int default_len;
    bool default_set;
} wpe_uniform_info;

typedef struct {
    const char* name;
    const char* default_texture;
    wpe_texture* default_texture_ref;
    int texture_slot;
} wpe_sampler_info;

typedef struct {
    const char* name;
    const char* type;
    int array_size;
} wpe_attribute_info;

typedef struct {
    const char* name;
    int texture_id;
    wpe_texture* texture;
    wpe_effect_fbo* fbo;
} wpe_material_texture;

typedef struct {
    const char* name;
    const float* values;
    int len;
} wpe_uniform_constant;

typedef struct {
    const char* name;
    int index;
    wpe_texture* texture;
    wpe_effect_fbo* fbo;
} wpe_material_pass_bind;

typedef struct {
    wpe_material_texture* textures;
    int num_textures;
    wpe_uniform_constant* constants;
    int num_constants;
    const char* target;
    wpe_effect_fbo* target_fbo;
    bool final_present;
    int max_texture_slot;
    wpe_material_pass_bind* binds;
    int num_binds;
} wpe_material_pass;

typedef struct wpe_texture {
    int id;
    const char* name;
    int width;
    int height;
    bool clamp_uv;
    bool interpolation;
    ow_texture_id texture;
} wpe_texture;

typedef struct {
    ow_texture_id texture;
    uint32_t width;
    uint32_t height;
    wpe_texture* source_texture;
} wpe_texture_target;

typedef struct {
    int id;
    const char* name;
    wpe_uniform_info* vertex_uniforms;
    int num_vertex_uniforms;
    wpe_uniform_info* fragment_uniforms;
    int num_fragment_uniforms;
    wpe_attribute_info* attributes;
    int num_attributes;
    wpe_sampler_info* samplers;
    int num_samplers;
    ow_vertex_shader_id vertex_shader;
    ow_fragment_shader_id fragment_shader;
} wpe_shader;

typedef struct {
    const char* blending;
    int shader_id;
    wpe_shader* shader;
    wpe_material_texture* textures;
    int num_textures;
    int max_texture_slot;
    ow_pipeline_id texture_pipeline;
    ow_pipeline_id disabled_texture_pipeline;
    ow_pipeline_id present_texture_pipeline;
    ow_pipeline_id mesh_pipeline;
    ow_pipeline_id disabled_mesh_pipeline;
    ow_pipeline_id present_mesh_pipeline;
} wpe_material;

typedef struct wpe_effect_fbo {
    const char* name;
    int scale;
    ow_texture_id texture;
    uint32_t width;
    uint32_t height;
} wpe_effect_fbo;

typedef struct {
    const char* name;
    bool visible;
    wpe_material_pass* passes;
    int num_passes;
    wpe_effect_fbo* fbos;
    int num_fbos;
    wpe_material* materials;
    int num_materials;
} wpe_image_effect;

typedef struct {
    wpe_texture_target previous;
    wpe_image_effect* final_effect;
    wpe_material_pass* final_pass;
    wpe_material* final_material;
    bool has_final_pass;
} wpe_effect_render_result;

typedef struct {
    wpe_vec3 color;
    int color_blend_mode;
    float alpha;
    float brightness;
    bool fullscreen;
    bool composition_layer;
    bool passthrough;
    wpe_material material;
    wpe_image_effect* effects;
    int num_effects;
    ow_texture_id ping_pong[2];
    uint32_t ping_pong_width;
    uint32_t ping_pong_height;
    wpe_material puppet_material;
    struct wpe_puppet_model* puppet;
} wpe_image_object;

typedef struct {
    float position[3];
    float texcoord[2];
    float normal[3];
    float tangent[4];
    uint32_t blend_indices[4];
    float blend_weights[4];
} wpe_puppet_vertex;

typedef struct {
    uint32_t start;
    uint32_t size;
} wpe_puppet_mesh_part;

typedef struct {
    char* material_name;
    wpe_puppet_vertex* vertices;
    int num_vertices;
    uint16_t* indices;
    int num_indices;
    wpe_puppet_mesh_part* parts;
    int num_parts;
    ow_vertex_buffer_id vertex_buffer;
    ow_index_buffer_id index_buffer;
} wpe_puppet_mesh;

typedef struct {
    uint32_t parent;
    wpe_mat4 local_bind;
    wpe_mat4 bind_pose;
    wpe_mat4 inv_bind;
} wpe_puppet_bone;

typedef struct {
    char* name;
    uint16_t bone_index;
    wpe_mat4 local_transform;
} wpe_puppet_attachment;

typedef struct {
    wpe_vec3 position;
    wpe_vec3 angle;
    wpe_vec3 scale;
} wpe_puppet_bone_frame;

typedef struct {
    wpe_puppet_bone_frame* frames;
    int num_frames;
} wpe_puppet_bone_track;

typedef enum {
    WPE_PUPPET_PLAY_LOOP,
    WPE_PUPPET_PLAY_MIRROR,
    WPE_PUPPET_PLAY_SINGLE,
} wpe_puppet_play_mode;

typedef struct {
    int id;
    wpe_puppet_play_mode mode;
    float fps;
    int length;
    wpe_puppet_bone_track* bone_tracks;
    int num_bone_tracks;
} wpe_puppet_animation;

typedef struct {
    int id;
    float blend;
    float rate;
    bool visible;
    bool additive;
    bool blend_in;
    bool blend_out;
    float blend_time;
} wpe_puppet_animation_layer;

typedef struct wpe_puppet_model {
    const char* path;
    wpe_puppet_mesh* meshes;
    int num_meshes;
    wpe_puppet_bone* bones;
    int num_bones;
    wpe_puppet_attachment* attachments;
    int num_attachments;
    wpe_puppet_animation* animations;
    int num_animations;
    wpe_puppet_animation_layer* layers;
    int num_layers;
    wpe_mat4* pose_matrices;
    wpe_mat4* bone_matrices;
} wpe_puppet_model;

typedef struct {
    bool alive;
    float position[3];
    float velocity[3];
    float rotation[3];
    float angular_velocity[3];
    float color[3];
    float initial_color[3];
    float alpha;
    float initial_alpha;
    float size;
    float initial_size;
    int frame;
    float lifetime;
    float age;
    float oscillate_random;
} wpe_particle_instance;

typedef struct {
    float position[3];
    float rotation[3];
    float size;
    float color[4];
    int frame;
} wpe_particle_instance_data;

typedef struct {
    float directions[3];
    float distance_max[3];
    float distance_min[3];
    float origin[3];
    int sign[3];
    float speed_min;
    float speed_max;
    float rate;
    float interval;
    float timer;
} wpe_particle_emitter;

typedef struct {
    int mode;
    float exponent;
    float bounds[2];
    int frequency_start;
    int frequency_end;
} wpe_turbulent_audio_response;

typedef struct {
    float min_lifetime;
    float max_lifetime;
    float min_size;
    float max_size;
    float min_velocity[3];
    float max_velocity[3];
    float min_rotation[3];
    float max_rotation[3];
    float min_angular_velocity[3];
    float max_angular_velocity[3];
    float min_color[3];
    float max_color[3];
    float min_alpha;
    float max_alpha;
    bool turbulent_velocity;
    float turbulent_scale;
    float turbulent_time_scale;
    float turbulent_offset;
    float turbulent_speed_min;
    float turbulent_speed_max;
    float turbulent_phase_min;
    float turbulent_phase_max;
    float turbulent_forward[3];
    float turbulent_right[3];
    wpe_turbulent_audio_response turbulent_audio;
} wpe_particle_initializer;

typedef struct {
    bool enabled;
    float gravity[3];
    float drag;
    float speed;
} wpe_particle_movement_operator;

typedef struct {
    bool enabled;
    float drag;
    float force[3];
} wpe_particle_angular_movement_operator;

typedef struct {
    bool enabled;
    float start_time;
    float end_time;
    float start_value;
    float end_value;
} wpe_particle_size_change_operator;

typedef struct {
    bool enabled;
    float start_time;
    float end_time;
    float start_value[3];
    float end_value[3];
} wpe_particle_color_change_operator;

typedef struct {
    bool enabled;
    float fade_in_time;
    float fade_out_time;
} wpe_particle_alpha_fade_operator;

typedef struct {
    bool enabled;
    float mask[3];
    float frequency_min;
    float frequency_max;
    float phase_min;
    float phase_max;
    float scale_min;
    float scale_max;
} wpe_particle_oscillate_position_operator;

typedef struct {
    bool enabled;
    float frequency_min;
    float frequency_max;
    float phase_min;
    float phase_max;
    float scale_min;
    float scale_max;
} wpe_particle_oscillate_scalar_operator;

typedef struct {
    wpe_particle_movement_operator movement;
    wpe_particle_angular_movement_operator angular_movement;
    wpe_particle_size_change_operator size_change;
    wpe_particle_color_change_operator color_change;
    wpe_particle_alpha_fade_operator alpha_fade;
    wpe_particle_oscillate_position_operator oscillate_position;
    wpe_particle_oscillate_scalar_operator oscillate_alpha;
    wpe_particle_oscillate_scalar_operator oscillate_size;
} wpe_particle_operator;

typedef struct {
    wpe_material material;
    wpe_particle_emitter* emitters;
    int num_emitters;
    wpe_particle_initializer init;
    wpe_particle_operator operator;
    int max_count;
    float start_time;
    float sequence_multiplier;
    bool random_frame;
    int spritesheet_cols;
    int spritesheet_rows;
    int spritesheet_frames;
    float spritesheet_duration;
    float texture_ratio;
    wpe_particle_instance* instances;
    wpe_particle_instance_data* instance_data;
    ow_vertex_buffer_id instance_buffer;
    ow_pipeline_id pipeline;
    int free_pos;
    int alive_count;
} wpe_particle_object;

typedef enum {
    OBJECTTYPE_IMAGE,
    OBJECTTYPE_PARTICLE,
    OBJECTTYPE_EMPTY,
} wpe_object_type;

typedef struct wpe_object {
    int id;
    int parent;
    struct wpe_object* parent_object;
    bool visible;
    const char* name;
    const char* attachment;
    wpe_vec3 origin;
    wpe_vec3 scale;
    wpe_vec3 angles;
    wpe_vec2 size;
    bool perspective;
    wpe_vec2 parallax_depth;
    wpe_object_type type;
    union {
        wpe_image_object image;
        wpe_particle_object particle;
    };
} wpe_object;

typedef struct {
    bool parallax;
    float parallax_amount;
    float parallax_delay;
    float parallax_mouse_influence;
    bool shake;
    float shake_amplitude;
    float shake_roughness;
    float shake_speed;
    bool clear_enabled;
    wpe_vec3 clear_color;
    wpe_vec2 ortho;
    float zoom;
    float fov;
    float near_z;
    float far_z;
} wpe_scene_general;

typedef struct {
    wpe_texture* textures;
    size_t num_textures;
    wpe_shader* shaders;
    size_t num_shaders;
    wpe_object* objects;
    size_t num_objects;
    wpe_scene_general general;
    int passthrough_shader_id;
    int audio_spectrum_size;
} wpe_scene;

typedef struct {
    float scene_width;
    float scene_height;
    float zoom;
    float origin_x;
    float origin_y;
    float origin_z;
    float size_x;
    float size_y;
    float scale_x;
    float scale_y;
    float scale_z;
    wpe_mat4 parent_model;
    int has_parent_model;
    float angle_x;
    float angle_y;
    float angle_z;
    float parallax_depth_x;
    float parallax_depth_y;
    int parallax_enabled;
    float parallax_amount;
    float parallax_mouse_influence;
    int perspective;
    float near_z;
    float far_z;
    float fov;
    int shake_enabled;
    float shake_amplitude;
    float shake_roughness;
    float shake_speed;
    wpe_scale_mode scale_mode;
    float mouse_x;
    float mouse_y;
    float time;
    int mesh_vertices;
} wpe_transform_parameters;

typedef struct {
    wpe_mat4 model;
    wpe_mat4 view_projection;
    wpe_mat4 model_view_projection;
    wpe_vec4 orientation_up;
    wpe_vec4 orientation_right;
    wpe_vec4 orientation_forward;
    float parallax_position_x;
    float parallax_position_y;
} wpe_transform_matrices;

typedef struct {
    uint32_t screen_width;
    uint32_t screen_height;
    float time_seconds;
    float mouse_x;
    float mouse_y;
    float parallax_mouse_x;
    float parallax_mouse_y;
    bool parallax_initialized;
    wpe_scale_mode scale_mode;
    float* audio_spectrum;
    int audio_spectrum_size;
} wpe_renderer_state;

#if defined(SCENE) && defined(WPE_COMPILE_SCENE_DEFINE)
#include "scene.h"
#else
extern wpe_scene scene;
#endif

wpe_mat4 wpe_mat4_identity();
wpe_mat4 wpe_mat4_inverse_affine(wpe_mat4 m);
wpe_transform_matrices wpe_default_transform_matrices();
wpe_transform_matrices wpe_compute_transform_matrices(wpe_transform_parameters params);

bool wpe_starts_with(const char* value, const char* prefix);
bool wpe_ends_with(const char* value, const char* suffix);
int wpe_texture_slot_from_uniform_name(const char* name);
int wpe_texture_slot_for_sampler(wpe_sampler_info* sampler, int fallback);

wpe_texture* wpe_find_texture(int id);
wpe_texture* wpe_find_texture_by_name(const char* name);
wpe_shader* wpe_find_shader(int id);
wpe_object* wpe_find_object(int id);
void wpe_resolve_object_parents();
wpe_texture* wpe_material_texture_at(wpe_material* material, int slot);
wpe_effect_fbo* wpe_find_effect_fbo(wpe_image_effect* effect, const char* name);

void wpe_renderer_common_init();
bool wpe_passthrough_available();
ow_blend_mode wpe_blend_mode_from_name(const char* name);
ow_sampler_id wpe_sampler_for_texture(wpe_texture* texture);
ow_pipeline_id wpe_passthrough_pipeline_for_blending(const char* blending, bool texture_target);
ow_pipeline_id wpe_create_mesh_pipeline(
    wpe_shader* shader, ow_blend_mode blend_mode, ow_texture_format color_target_format);
void wpe_init_texture(wpe_texture* texture);
void wpe_init_shader(wpe_shader* shader);
void wpe_init_material(wpe_material* material);
void wpe_init_material_pass(wpe_material_pass* pass, wpe_image_effect* effect);
void wpe_init_effect_present_pipeline(wpe_material* material, const char* blending);
void wpe_ensure_texture_size(
    ow_texture_id* texture, uint32_t* current_width, uint32_t* current_height, uint32_t width, uint32_t height);
wpe_transform_parameters wpe_transform_parameters_for_object(wpe_object* object, const wpe_renderer_state* state);
wpe_transform_parameters wpe_transform_parameters_for_object_mesh(wpe_object* object, const wpe_renderer_state* state);

uint8_t* wpe_build_uniform_data(wpe_uniform_info* uniforms, int num_uniforms, wpe_object* object,
    wpe_texture_target* texture_slots, int num_texture_slots, wpe_uniform_constant* constants, int num_constants,
    wpe_transform_matrices matrices, const wpe_renderer_state* state, int* size_out);
float wpe_audio_spectrum_value(const wpe_renderer_state* state, int target_size, int index);

bool wpe_render_material_to_target(wpe_object* object, wpe_material* material, wpe_material_pass* pass,
    ow_pipeline_id pipeline, ow_texture_id color_target, bool clear, wpe_texture_target previous, bool default_previous,
    wpe_transform_matrices matrices, const wpe_renderer_state* state);
bool wpe_render_material_mesh_to_target(wpe_object* object, wpe_material* material, wpe_material_pass* pass,
    ow_pipeline_id pipeline, wpe_puppet_mesh* mesh, ow_texture_id color_target, bool clear, wpe_texture_target previous,
    bool default_previous, wpe_transform_matrices matrices, const wpe_renderer_state* state);
bool wpe_render_texture_with_passthrough(wpe_object* object, wpe_texture_target source, ow_pipeline_id pipeline,
    ow_texture_id color_target, bool clear, wpe_transform_matrices matrices, const wpe_renderer_state* state);

wpe_texture_target wpe_current_screen_snapshot();
wpe_texture_target wpe_next_screen_snapshot();
bool wpe_screen_snapshot_available();
void wpe_commit_screen_snapshot();
bool wpe_prepare_next_screen_snapshot(wpe_object* object, bool clear, const wpe_renderer_state* state);

void wpe_renderer_init();
void wpe_renderer_begin_frame(const wpe_renderer_state* state);
void wpe_renderer_init_object(wpe_object* obj);
void wpe_renderer_init_image_object(wpe_object* obj);
void wpe_init_puppet_model(wpe_puppet_model* puppet);
bool wpe_puppet_attachment_transform(wpe_puppet_model* puppet, const char* name, wpe_mat4* transform_out);
void wpe_renderer_init_particle_object(wpe_object* obj);
void wpe_renderer_update_particle_objects(float delta, const wpe_renderer_state* state);
bool wpe_renderer_render_image_object(wpe_object* object, bool clear, const wpe_renderer_state* state);
bool wpe_renderer_render_particle_object(wpe_object* object, bool clear, const wpe_renderer_state* state);
void wpe_renderer_end_frame(const wpe_renderer_state* state);

#endif
