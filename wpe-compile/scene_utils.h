#ifndef WPE_COMPILE_SCENE_UTILS_H
#define WPE_COMPILE_SCENE_UTILS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "openwallpaper.h"

typedef struct {
    float at[1];
} glsl_float;

typedef struct {
    float at[2];
} glsl_vec2;

typedef struct {
    float at[3];
} glsl_vec3;

typedef struct {
    float at[4];
} glsl_vec4;

typedef struct {
    float at[3][4];
} glsl_mat3;

typedef struct {
    float at[4][4];
} glsl_mat4;

typedef enum {
    SCALE_MODE_STRETCH,
    SCALE_MODE_ASPECT_FIT,
    SCALE_MODE_ASPECT_CROP,
} scale_mode_t;

typedef struct {
    float scene_width;
    float scene_height;
    float origin_x;
    float origin_y;
    float origin_z;
    float size_x;
    float size_y;
    float scale_x;
    float scale_y;
    float scale_z;
    float parallax_depth_x;
    float parallax_depth_y;
    int parallax_enabled;
    float parallax_amount;
    float parallax_mouse_influence;
    int perspective;
    float near_z;
    float far_z;
    float fov;
    scale_mode_t scale_mode;
    float mouse_x;
    float mouse_y;
} transform_parameters_t;

typedef struct {
    glsl_mat4 model;
    glsl_mat4 view_projection;
    glsl_mat4 model_view_projection;
    float parallax_position_x;
    float parallax_position_y;
} transform_matrices_t;

static glsl_mat4 mat4_identity() {
    glsl_mat4 res;
    res.at[0][0] = 1.0f;
    res.at[0][1] = 0.0f;
    res.at[0][2] = 0.0f;
    res.at[0][3] = 0.0f;
    res.at[1][0] = 0.0f;
    res.at[1][1] = 1.0f;
    res.at[1][2] = 0.0f;
    res.at[1][3] = 0.0f;
    res.at[2][0] = 0.0f;
    res.at[2][1] = 0.0f;
    res.at[2][2] = 1.0f;
    res.at[2][3] = 0.0f;
    res.at[3][0] = 0.0f;
    res.at[3][1] = 0.0f;
    res.at[3][2] = 0.0f;
    res.at[3][3] = 1.0f;
    return res;
}

static glsl_mat4 mat4_scale_xy(float sx, float sy) {
    glsl_mat4 m = mat4_identity();
    m.at[0][0] = sx;
    m.at[1][1] = sy;
    return m;
}

static glsl_mat4 mat4_multiply(glsl_mat4 a, glsl_mat4 b) {
    glsl_mat4 res;
    for(int col = 0; col < 4; ++col) {
        for(int row = 0; row < 4; ++row) {
            res.at[col][row] = 0;
            for(int k = 0; k < 4; ++k) {
                res.at[col][row] += a.at[k][row] * b.at[col][k];
            }
        }
    }
    return res;
}

static glsl_vec3 vec3_sub(glsl_vec3 a, glsl_vec3 b) {
    glsl_vec3 res = {{a.at[0] - b.at[0], a.at[1] - b.at[1], a.at[2] - b.at[2]}};
    return res;
}

static glsl_vec3 vec3_cross(glsl_vec3 a, glsl_vec3 b) {
    glsl_vec3 res = {{
        a.at[1] * b.at[2] - a.at[2] * b.at[1],
        a.at[2] * b.at[0] - a.at[0] * b.at[2],
        a.at[0] * b.at[1] - a.at[1] * b.at[0],
    }};
    return res;
}

static float vec3_dot(glsl_vec3 a, glsl_vec3 b) {
    return a.at[0] * b.at[0] + a.at[1] * b.at[1] + a.at[2] * b.at[2];
}

static glsl_vec3 vec3_normalize(glsl_vec3 v) {
    float len = sqrtf(vec3_dot(v, v));
    if(len > 0.00001f) {
        float inv_len = 1.0f / len;
        v.at[0] *= inv_len;
        v.at[1] *= inv_len;
        v.at[2] *= inv_len;
    }
    return v;
}

static glsl_mat4 mat4_look_at(glsl_vec3 eye, glsl_vec3 center, glsl_vec3 up) {
    glsl_vec3 f = vec3_normalize(vec3_sub(center, eye));
    glsl_vec3 s = vec3_normalize(vec3_cross(f, up));
    glsl_vec3 u = vec3_cross(s, f);

    glsl_mat4 res = mat4_identity();
    res.at[0][0] = s.at[0];
    res.at[1][0] = s.at[1];
    res.at[2][0] = s.at[2];

    res.at[0][1] = u.at[0];
    res.at[1][1] = u.at[1];
    res.at[2][1] = u.at[2];

    res.at[0][2] = -f.at[0];
    res.at[1][2] = -f.at[1];
    res.at[2][2] = -f.at[2];

    res.at[3][0] = -vec3_dot(s, eye);
    res.at[3][1] = -vec3_dot(u, eye);
    res.at[3][2] = vec3_dot(f, eye);
    return res;
}

static glsl_mat4 mat4_perspective(float fov_radians, float aspect, float near_z, float far_z) {
    if(fov_radians == 0.0f || aspect == 0.0f) {
        return mat4_identity();
    }
    if(near_z <= 0.0f) {
        near_z = 0.01f;
    }
    if(far_z <= near_z) {
        far_z = near_z + 1.0f;
    }

    float f = 1.0f / tanf(fov_radians * 0.5f);
    glsl_mat4 res = {0};
    res.at[0][0] = f / aspect;
    res.at[1][1] = f;
    res.at[2][2] = (far_z + near_z) / (near_z - far_z);
    res.at[2][3] = -1.0f;
    res.at[3][2] = (2.0f * far_z * near_z) / (near_z - far_z);
    return res;
}

static transform_matrices_t default_transform_matrices() {
    transform_matrices_t res;
    res.model = mat4_identity();
    res.view_projection = mat4_identity();
    res.model_view_projection = mat4_identity();
    res.parallax_position_x = 0.5f;
    res.parallax_position_y = 0.5f;
    return res;
}

static transform_matrices_t compute_transform_matrices(transform_parameters_t params) {
    transform_matrices_t res = default_transform_matrices();

    uint32_t screen_width = 0;
    uint32_t screen_height = 0;
    ow_get_screen_size(&screen_width, &screen_height);
    if(screen_width == 0 || screen_height == 0 || params.scene_width == 0.0f || params.scene_height == 0.0f) {
        return res;
    }

    float cam_width = params.scene_width;
    float cam_height = params.scene_height;
    float screen_aspect = (float)screen_width / (float)screen_height;
    float scene_aspect = params.scene_width / params.scene_height;

    if(params.scale_mode == SCALE_MODE_ASPECT_FIT) {
        if(screen_aspect < scene_aspect) {
            cam_width = params.scene_width;
            cam_height = params.scene_width / screen_aspect;
        } else {
            cam_width = params.scene_height * screen_aspect;
            cam_height = params.scene_height;
        }
    } else if(params.scale_mode == SCALE_MODE_ASPECT_CROP) {
        if(screen_aspect > scene_aspect) {
            cam_width = params.scene_width;
            cam_height = params.scene_width / screen_aspect;
        } else {
            cam_width = params.scene_height * screen_aspect;
            cam_height = params.scene_height;
        }
    }

    float vp_scale_x = 2.0f / cam_width;
    float vp_scale_y = 2.0f / cam_height;
    glsl_mat4 vp = mat4_identity();

    if(params.perspective) {
        float fov_radians = params.fov * (float)M_PI / 180.0f;
        float camera_distance = (cam_height * 0.5f) / tanf(fov_radians * 0.5f);
        glsl_vec3 eye = {{params.scene_width * 0.5f, params.scene_height * 0.5f, camera_distance}};
        glsl_vec3 center = {{params.scene_width * 0.5f, params.scene_height * 0.5f, 0.0f}};
        glsl_vec3 up = {{0.0f, 1.0f, 0.0f}};
        glsl_mat4 view = mat4_look_at(eye, center, up);
        glsl_mat4 proj = mat4_perspective(fov_radians, scene_aspect, params.near_z, params.far_z);
        float sx = 1.0f;
        float sy = 1.0f;

        if(params.scale_mode == SCALE_MODE_ASPECT_FIT) {
            if(screen_aspect > scene_aspect) {
                sx = scene_aspect / screen_aspect;
            } else {
                sy = screen_aspect / scene_aspect;
            }
        } else if(params.scale_mode == SCALE_MODE_ASPECT_CROP) {
            if(screen_aspect > scene_aspect) {
                sy = screen_aspect / scene_aspect;
            } else {
                sx = scene_aspect / screen_aspect;
            }
        } else {
            sx = 1.0f;
            sy = 1.0f;
        }

        glsl_mat4 aspect_fix = mat4_scale_xy(sx, sy);
        glsl_mat4 pv = mat4_multiply(proj, view);
        vp = mat4_multiply(aspect_fix, pv);
    } else {
        vp.at[0][0] = vp_scale_x;
        vp.at[0][1] = 0.0f;
        vp.at[0][2] = 0.0f;
        vp.at[0][3] = 0.0f;

        vp.at[1][0] = 0.0f;
        vp.at[1][1] = vp_scale_y;
        vp.at[1][2] = 0.0f;
        vp.at[1][3] = 0.0f;

        vp.at[2][0] = 0.0f;
        vp.at[2][1] = 0.0f;
        vp.at[2][2] = 1.0f;
        vp.at[2][3] = 0.0f;

        vp.at[3][0] = -params.scene_width * vp_scale_x * 0.5f;
        vp.at[3][1] = -params.scene_height * vp_scale_y * 0.5f;
        vp.at[3][2] = 0.0f;
        vp.at[3][3] = 1.0f;
    }

    float clamped_mouse_x = params.mouse_x;
    float clamped_mouse_y = params.mouse_y;
    if(clamped_mouse_x < 0.0f) {
        clamped_mouse_x = 0.0f;
    }
    if(clamped_mouse_x > 1.0f) {
        clamped_mouse_x = 1.0f;
    }
    if(clamped_mouse_y < 0.0f) {
        clamped_mouse_y = 0.0f;
    }
    if(clamped_mouse_y > 1.0f) {
        clamped_mouse_y = 1.0f;
    }

    float parallax_pos_x = 0.5f;
    float parallax_pos_y = 0.5f;
    if(params.parallax_enabled) {
        float diff_x = clamped_mouse_x - 0.5f;
        float diff_y = -clamped_mouse_y - 0.5f;
        parallax_pos_x = 0.5f + diff_x * params.parallax_mouse_influence;
        parallax_pos_y = 0.5f + diff_y * params.parallax_mouse_influence;
    }

    float node_pos_x = params.origin_x;
    float node_pos_y = params.origin_y;
    float cam_pos_x = params.scene_width * 0.5f;
    float cam_pos_y = params.scene_height * 0.5f;
    float parallax_offset_x = 0.0f;
    float parallax_offset_y = 0.0f;

    if(params.parallax_enabled) {
        float mouse_vec_x = 0.5f - clamped_mouse_x;
        float mouse_vec_y = clamped_mouse_y - 0.5f;
        mouse_vec_x *= params.scene_width * params.parallax_mouse_influence;
        mouse_vec_y *= params.scene_height * params.parallax_mouse_influence;
        float depth_x = params.parallax_depth_x;
        float depth_y = params.parallax_depth_y;
        float dx = (node_pos_x - cam_pos_x + mouse_vec_x) * depth_x * params.parallax_amount;
        float dy = (node_pos_y - cam_pos_y + mouse_vec_y) * depth_y * params.parallax_amount;
        parallax_offset_x = dx;
        parallax_offset_y = dy;
    }

    float tx = params.origin_x + parallax_offset_x;
    float ty = params.origin_y + parallax_offset_y;
    float tz = params.origin_z;
    float sx = 0.5f * params.size_x * params.scale_x;
    float sy = 0.5f * params.size_y * params.scale_y;
    float sz = params.scale_z;

    glsl_mat4 model = mat4_identity();
    model.at[0][0] = sx;
    model.at[1][1] = sy;
    model.at[2][2] = sz;
    model.at[3][0] = tx;
    model.at[3][1] = ty;
    model.at[3][2] = tz;

    res.model = model;
    res.view_projection = vp;
    res.model_view_projection = mat4_multiply(vp, model);
    res.parallax_position_x = parallax_pos_x;
    res.parallax_position_y = parallax_pos_y;
    return res;
}

typedef struct {
    bool alive;

    float position[3];
    float velocity[3];
    float acceleration[3];

    float rotation;
    float angular_velocity[3];
    float angular_acceleration[3];

    float color[3];
    float alpha;
    float size;
    float frame;

    float lifetime;
    float age;
} particle_instance_t;

typedef struct {
    float position[3];
    float rotation;
    float size;
    float color[4];
    float frame;
} particle_instance_data_t;

typedef enum {
    PARTICLE_EMITTER_TYPE_SPHERERANDOM,
} particle_emitter_type;

typedef struct {
    particle_emitter_type type;
    float directions[3];
    float distance_max[3];
    float distance_min[3];
    float origin[3];
    int sign[3];
    float speed_min;
    float speed_max;
    float interval;
    float timer;
} particle_emitter_t;

typedef struct {
    float min_lifetime;
    float max_lifetime;
    float min_size;
    float max_size;
    float min_velocity[3];
    float max_velocity[3];
    float min_color[3];
    float max_color[3];
} particle_initializer_t;

typedef struct {
    bool movement;
    float gravity[3];
} particle_operator_t;

typedef struct {
    particle_instance_t* instances;
    particle_instance_data_t* instance_data;
    ow_id instance_buffer;
    particle_emitter_t* emitters;
    particle_initializer_t init;
    particle_operator_t operator;
    int max_count;
    int emitter_count;
    float origin[3];
    int free_pos;
    int alive_count;
} particle_t;

void init_particle(particle_t* particle) {
    particle->instances = (particle_instance_t*)calloc(particle->max_count, sizeof(particle_instance_t));
    particle->instance_data = (particle_instance_data_t*)calloc(particle->max_count, sizeof(particle_instance_data_t));
}

float rand_float(float min, float max) {
    return min + (max - min) * (float)rand() / RAND_MAX;
}

void spawn_particle_instance(particle_t* particle, particle_emitter_t* emitter) {
    if(particle->alive_count == particle->max_count) {
        return;
    }
    while(particle->instances[particle->free_pos].alive) {
        particle->free_pos++;
        if(particle->free_pos >= particle->max_count) {
            particle->free_pos = 0;
        }
    }

    particle_instance_t* instance = &particle->instances[particle->free_pos];
    instance->alive = true;
    particle->alive_count++;

    switch(emitter->type) {
        case PARTICLE_EMITTER_TYPE_SPHERERANDOM: {
            for(int it = 0; it < 10; it++) {
                float dist = 0.0f;
                for(int i = 0; i < 3; i++) {
                    float offset = rand_float(emitter->distance_min[i], emitter->distance_max[i]);
                    dist += (offset / emitter->distance_max[i]) * (offset / emitter->distance_max[i]);
                    offset *= emitter->directions[i];
                    if(rand() % 2) {
                        offset = -offset;
                    }
                    if((emitter->sign[i] > 0 && offset < 0.0f) || (emitter->sign[i] < 0 && offset > 0.0f)) {
                        offset = -offset;
                    }
                    instance->position[i] = emitter->origin[i] + offset;
                }
                if(dist <= 1.0f) {
                    break;
                }
            }
        }
        default:
            break;
    }

    instance->size = rand_float(particle->init.min_size, particle->init.max_size);
    instance->size /= 2.0f;

    float factor = rand_float(0.0f, 1.0f);
    for(int i = 0; i < 3; i++) {
        instance->velocity[i] =
            particle->init.min_velocity[i] + (particle->init.max_velocity[i] - particle->init.min_velocity[i]) * factor;
    }

    instance->lifetime = rand_float(particle->init.min_lifetime, particle->init.max_lifetime);

    factor = rand_float(0.0f, 1.0f);
    for(int i = 0; i < 3; i++) {
        instance->color[i] =
            particle->init.min_color[i] + (particle->init.max_color[i] - particle->init.min_color[i]) * factor;
    }

    instance->alpha = 1.0f;
    instance->age = 0.0f;
}

void update_particle_instance(particle_t* particle, particle_instance_t* instance, float delta) {
    if(!instance->alive) {
        return;
    }
    instance->age += delta;
    if(instance->age >= instance->lifetime) {
        instance->alive = false;
        particle->alive_count--;
        return;
    }

    if(particle->operator.movement) {
        for(int i = 0; i < 3; i++) {
            instance->velocity[i] += particle->operator.gravity[i] * delta;
            instance->position[i] += instance->velocity[i] * delta;
        }
    }
}

void update_particle(particle_t* particle, float delta) {
    for(int i = 0; i < particle->emitter_count; i++) {
        particle->emitters[i].timer += delta;
        while(particle->emitters[i].timer >= particle->emitters[i].interval) {
            particle->emitters[i].timer -= particle->emitters[i].interval;
            spawn_particle_instance(particle, &particle->emitters[i]);
        }
    }
    for(int i = 0; i < particle->max_count; i++) {
        update_particle_instance(particle, &particle->instances[i], delta);
    }
}

void update_particle_instance_data(particle_t* particle) {
    for(int i = 0; i < particle->max_count; i++) {
        if(!particle->instances[i].alive) {
            particle->instance_data[i] = (particle_instance_data_t){0};
            continue;
        }
        particle->instance_data[i] = (particle_instance_data_t){
            .position =
                {
                    particle->instances[i].position[0],
                    particle->instances[i].position[1],
                    particle->instances[i].position[2],
                },
            .rotation = particle->instances[i].rotation,
            .size = particle->instances[i].size,
            .color =
                {
                    particle->instances[i].color[0],
                    particle->instances[i].color[1],
                    particle->instances[i].color[2],
                    particle->instances[i].alpha,
                },
            .frame = particle->instances[i].frame,
        };
    }
}

#endif
