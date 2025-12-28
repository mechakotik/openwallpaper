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
    int at[2];
} glsl_ivec2;

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
        float camera_distance = (params.scene_height * 0.5f) / tanf(fov_radians * 0.5f);
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
    float oscillate_frequency[3];
    float oscillate_scale[3];
    float oscillate_phase[3];

    float rotation;
    float angular_velocity[3];
    float angular_acceleration[3];

    float color[3];
    float alpha;
    float initial_alpha;
    float size;
    int frame;

    float lifetime;
    float age;
} particle_instance_t;

typedef struct {
    float position[3];
    float rotation;
    float size;
    float color[4];
    int frame;
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
    bool turbulent_velocity;
    bool turbulent_noise_initialized;
    float turbulent_scale;
    float turbulent_timescale;
    float turbulent_offset;
    float turbulent_speed_min;
    float turbulent_speed_max;
    float turbulent_phase_min;
    float turbulent_phase_max;
    float turbulent_forward[3];
    float turbulent_right[3];
    float turbulent_up[3];
    float turbulent_noise_pos[3];
} particle_initializer_t;

typedef struct {
    bool movement;
    float gravity[3];
    float drag;
    float speed;
    bool oscillate_position;
    float oscillate_mask[3];
    float oscillate_frequency_min;
    float oscillate_frequency_max;
    float oscillate_scale_min;
    float oscillate_scale_max;
    float oscillate_phase_min;
    float oscillate_phase_max;
    bool alpha_fade;
    float alpha_fade_in_time;
    float alpha_fade_out_time;
} particle_operator_t;

typedef enum {
    PARTICLE_ANIMATION_SEQUENCE = 0,
    PARTICLE_ANIMATION_RANDOM_FRAME = 1,
    PARTICLE_ANIMATION_ONCE = 2,
} particle_animation_mode_t;

typedef struct {
    particle_instance_t* instances;
    particle_instance_data_t* instance_data;
    ow_id instance_buffer;
    particle_emitter_t* emitters;
    particle_initializer_t init;
    particle_operator_t operator;
    int spritesheet_cols;
    int spritesheet_rows;
    int spritesheet_frames;
    float spritesheet_duration;
    float sequence_multiplier;
    int animation_mode;
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

float fade_value(float life, float start, float end, float start_value, float end_value) {
    if(life <= start) {
        return start_value;
    }
    if(life > end) {
        return end_value;
    }
    float span = end - start;
    if(fabsf(span) < 0.00001f) {
        return end_value;
    }
    float pass = (life - start) / span;
    return start_value + (end_value - start_value) * pass;
}

static float clampf(float value, float min, float max) {
    if(value < min) {
        return min;
    }
    if(value > max) {
        return max;
    }
    return value;
}

static float vec3_lengthf(const float v[3]) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void vec3_normalizef(float v[3]) {
    float len = vec3_lengthf(v);
    if(len > 0.0001f) {
        float inv = 1.0f / len;
        v[0] *= inv;
        v[1] *= inv;
        v[2] *= inv;
    }
}

static float vec3_dotf(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void vec3_crossf(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static void vec3_copy(const float src[3], float dst[3]) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void axis_angle_rotate(const float v[3], const float axis_in[3], float angle, float out[3]) {
    float axis[3];
    vec3_copy(axis_in, axis);
    vec3_normalizef(axis);
    float c = cosf(angle);
    float s = sinf(angle);
    float dot = vec3_dotf(axis, v);
    float cross[3];
    vec3_crossf(axis, v, cross);
    out[0] = v[0] * c + cross[0] * s + axis[0] * dot * (1.0f - c);
    out[1] = v[1] * c + cross[1] * s + axis[1] * dot * (1.0f - c);
    out[2] = v[2] * c + cross[2] * s + axis[2] * dot * (1.0f - c);
}

static const int PERLIN_PERM[512] = {151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36,
    103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203,
    117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48,
    27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143,
    54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207,
    206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153,
    101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
    228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214,
    31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29,
    24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180, 151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194,
    233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62,
    94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74,
    165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46,
    245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135,
    130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126,
    255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44,
    154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185,
    112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14,
    239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236,
    205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180};

static float perlin_grad(int hash, float x, float y, float z) {
    switch(hash & 0xF) {
        case 0x0:
            return x + y;
        case 0x1:
            return -x + y;
        case 0x2:
            return x - y;
        case 0x3:
            return -x - y;
        case 0x4:
            return x + z;
        case 0x5:
            return -x + z;
        case 0x6:
            return x - z;
        case 0x7:
            return -x - z;
        case 0x8:
            return y + z;
        case 0x9:
            return -y + z;
        case 0xA:
            return y - z;
        case 0xB:
            return -y - z;
        case 0xC:
            return y + x;
        case 0xD:
            return -y + z;
        case 0xE:
            return y - x;
        case 0xF:
            return -y - z;
        default:
            return 0.0f;
    }
}

static float perlin_fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float perlin_lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static float perlin_noise(float x, float y, float z) {
    int X = ((int)floorf(x)) & 255;
    int Y = ((int)floorf(y)) & 255;
    int Z = ((int)floorf(z)) & 255;

    x -= floorf(x);
    y -= floorf(y);
    z -= floorf(z);

    float u = perlin_fade(x);
    float v = perlin_fade(y);
    float w = perlin_fade(z);

    int A = PERLIN_PERM[X] + Y;
    int AA = PERLIN_PERM[A] + Z;
    int AB = PERLIN_PERM[A + 1] + Z;
    int B = PERLIN_PERM[X + 1] + Y;
    int BA = PERLIN_PERM[B] + Z;
    int BB = PERLIN_PERM[B + 1] + Z;

    return perlin_lerp(w,
        perlin_lerp(v, perlin_lerp(u, perlin_grad(PERLIN_PERM[AA], x, y, z), perlin_grad(PERLIN_PERM[BA], x - 1, y, z)),
            perlin_lerp(u, perlin_grad(PERLIN_PERM[AB], x, y - 1, z), perlin_grad(PERLIN_PERM[BB], x - 1, y - 1, z))),
        perlin_lerp(v,
            perlin_lerp(
                u, perlin_grad(PERLIN_PERM[AA + 1], x, y, z - 1), perlin_grad(PERLIN_PERM[BA + 1], x - 1, y, z - 1)),
            perlin_lerp(u, perlin_grad(PERLIN_PERM[AB + 1], x, y - 1, z - 1),
                perlin_grad(PERLIN_PERM[BB + 1], x - 1, y - 1, z - 1))));
}

static void perlin_noise_vec3(const float p[3], float out[3]) {
    out[0] = perlin_noise(p[0], p[1], p[2]);
    out[1] = perlin_noise(p[0] + 89.2f, p[1] + 33.1f, p[2] + 57.3f);
    out[2] = perlin_noise(p[0] + 100.3f, p[1] + 120.1f, p[2] + 142.2f);
}

static void curl_noise_vec3(const float p[3], float out[3]) {
    const float e = 1e-4f;
    float dx[3] = {e, 0.0f, 0.0f};
    float dy[3] = {0.0f, e, 0.0f};
    float dz[3] = {0.0f, 0.0f, e};

    float x0[3] = {p[0] - dx[0], p[1] - dx[1], p[2] - dx[2]};
    float x1[3] = {p[0] + dx[0], p[1] + dx[1], p[2] + dx[2]};
    float y0[3] = {p[0] - dy[0], p[1] - dy[1], p[2] - dy[2]};
    float y1[3] = {p[0] + dy[0], p[1] + dy[1], p[2] + dy[2]};
    float z0[3] = {p[0] - dz[0], p[1] - dz[1], p[2] - dz[2]};
    float z1[3] = {p[0] + dz[0], p[1] + dz[1], p[2] + dz[2]};

    float nx0[3];
    perlin_noise_vec3(x0, nx0);
    float nx1[3];
    perlin_noise_vec3(x1, nx1);
    float ny0[3];
    perlin_noise_vec3(y0, ny0);
    float ny1[3];
    perlin_noise_vec3(y1, ny1);
    float nz0[3];
    perlin_noise_vec3(z0, nz0);
    float nz1[3];
    perlin_noise_vec3(z1, nz1);

    out[0] = (ny1[2] - ny0[2]) - (nz1[1] - nz0[1]);
    out[1] = (nz1[0] - nz0[0]) - (nx1[2] - nx0[2]);
    out[2] = (nx1[1] - nx0[1]) - (ny1[0] - ny0[0]);

    out[0] /= (2.0f * e);
    out[1] /= (2.0f * e);
    out[2] /= (2.0f * e);
}

static void generate_turbulent_velocity(particle_initializer_t* init, float duration, float out_velocity[3]) {
    float speed = rand_float(init->turbulent_speed_min, init->turbulent_speed_max);

    if(!init->turbulent_noise_initialized) {
        init->turbulent_noise_pos[0] = rand_float(0.0f, 10.0f);
        init->turbulent_noise_pos[1] = rand_float(0.0f, 10.0f);
        init->turbulent_noise_pos[2] = rand_float(0.0f, 10.0f);
        init->turbulent_noise_initialized = true;
    }

    float position[3];
    vec3_copy(init->turbulent_noise_pos, position);

    float step_duration = duration;
    if(step_duration > 10.0f) {
        position[0] += speed;
        step_duration = 0.0f;
    }

    float direction[3] = {1.0f, 0.0f, 0.0f};
    float time_scale = init->turbulent_timescale;
    if(fabsf(time_scale) < 0.0001f) {
        time_scale = 1.0f;
    }

    do {
        curl_noise_vec3(position, direction);
        vec3_normalizef(direction);
        float step = 0.005f / time_scale;
        position[0] += direction[0] * step;
        position[1] += direction[1] * step;
        position[2] += direction[2] * step;
        step_duration -= 0.01f;
    } while(step_duration > 0.01f);

    float forward_len = vec3_lengthf(init->turbulent_forward);
    float dir_len = vec3_lengthf(direction);
    if(forward_len > 0.0001f && dir_len > 0.0001f) {
        float dot = vec3_dotf(direction, init->turbulent_forward) / (forward_len * dir_len);
        dot = clampf(dot, -1.0f, 1.0f);
        float angle_ratio = acosf(dot) / (float)M_PI;
        float clamp_scale = init->turbulent_scale * 0.5f;
        if(clamp_scale < 0.0f) {
            clamp_scale = 0.0f;
        }
        if(angle_ratio > clamp_scale && clamp_scale < 1.0f) {
            float axis[3];
            vec3_crossf(direction, init->turbulent_forward, axis);
            if(vec3_lengthf(axis) > 0.0001f) {
                float rotate_angle = (angle_ratio - angle_ratio * clamp_scale) * (float)M_PI;
                axis_angle_rotate(direction, axis, rotate_angle, direction);
            }
        }
    }

    float rotated[3];
    if(vec3_lengthf(init->turbulent_right) > 0.0001f && fabsf(init->turbulent_offset) > 0.0001f) {
        axis_angle_rotate(direction, init->turbulent_right, init->turbulent_offset, rotated);
    } else {
        vec3_copy(direction, rotated);
    }

    out_velocity[0] = rotated[0] * speed;
    out_velocity[1] = rotated[1] * speed;
    out_velocity[2] = rotated[2] * speed;

    vec3_copy(position, init->turbulent_noise_pos);
}

void spawn_particle_instance(particle_t* particle, particle_emitter_t* emitter, float duration) {
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
    instance->frame = -1;

    float factor = rand_float(0.0f, 1.0f);
    for(int i = 0; i < 3; i++) {
        instance->velocity[i] =
            particle->init.min_velocity[i] + (particle->init.max_velocity[i] - particle->init.min_velocity[i]) * factor;
    }
    if(particle->init.turbulent_velocity) {
        float turbulent_velocity[3];
        generate_turbulent_velocity(&particle->init, duration, turbulent_velocity);
        for(int i = 0; i < 3; i++) {
            instance->velocity[i] += turbulent_velocity[i];
        }
    }
    for(int i = 0; i < 3; i++) {
        instance->oscillate_frequency[i] = 0.0f;
        instance->oscillate_scale[i] = 0.0f;
        instance->oscillate_phase[i] = 0.0f;
    }
    if(particle->operator.oscillate_position) {
        float frequency_max = particle->operator.oscillate_frequency_max;
        if(frequency_max == 0.0f) {
            frequency_max = particle->operator.oscillate_frequency_min;
        }
        float phase_max = particle->operator.oscillate_phase_max + 2.0f *(float) M_PI;
        for(int i = 0; i < 3; i++) {
            if(fabsf(particle->operator.oscillate_mask[i]) < 0.01f) {
                continue;
            }
            instance->oscillate_frequency[i] = rand_float(particle->operator.oscillate_frequency_min, frequency_max);
            instance->oscillate_scale[i] =
                rand_float(particle->operator.oscillate_scale_min, particle->operator.oscillate_scale_max);
            instance->oscillate_phase[i] = rand_float(particle->operator.oscillate_phase_min, phase_max);
        }
    }

    instance->lifetime = rand_float(particle->init.min_lifetime, particle->init.max_lifetime);

    factor = rand_float(0.0f, 1.0f);
    for(int i = 0; i < 3; i++) {
        instance->color[i] =
            particle->init.min_color[i] + (particle->init.max_color[i] - particle->init.min_color[i]) * factor;
    }

    instance->alpha = 1.0f;
    instance->initial_alpha = instance->alpha;
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
        float movement_speed = particle->operator.speed;
        if(fabsf(movement_speed) < 0.0001f) {
            movement_speed = 1.0f;
        }
        float drag_coeff = -2.0f * particle->operator.drag;
        for(int i = 0; i < 3; i++) {
            float acceleration = particle->operator.gravity[i] + drag_coeff * instance->velocity[i];
            instance->velocity[i] += acceleration * movement_speed * delta;
            instance->position[i] += instance->velocity[i] * delta;
        }
    }
    if(particle->operator.oscillate_position) {
        float time = instance->age - delta;
        if(time < 0.0f) {
            time = 0.0f;
        }
        for(int i = 0; i < 3; i++) {
            if(fabsf(particle->operator.oscillate_mask[i]) < 0.01f) {
                continue;
            }
            float frequency = instance->oscillate_frequency[i];
            float scale = instance->oscillate_scale[i];
            float phase = instance->oscillate_phase[i];
            float delta_pos = -1.0f * scale * frequency * sinf(frequency * time + phase) * delta;
            instance->position[i] += delta_pos;
        }
    }

    if(particle->operator.alpha_fade && instance->lifetime> 0.0f) {
        float life = instance->age / instance->lifetime;
        float fade = 1.0f;
        if(life <= particle->operator.alpha_fade_in_time) {
            fade = fade_value(life, 0.0f, particle->operator.alpha_fade_in_time, 0.0f, 1.0f);
        } else if(life > particle->operator.alpha_fade_out_time) {
            fade = 1.0f - fade_value(life, particle->operator.alpha_fade_out_time, 1.0f, 0.0f, 1.0f);
        }
        if(fade < 0.0f) {
            fade = 0.0f;
        } else if(fade > 1.0f) {
            fade = 1.0f;
        }
        instance->alpha = instance->initial_alpha * fade;
    }

    if(particle->spritesheet_frames > 0) {
        float anim_speed = particle->sequence_multiplier;
        if(fabsf(anim_speed) < 0.0001f) {
            anim_speed = 1.0f;
        }
        float frame_count = (float)particle->spritesheet_frames;
        float lifetime_pos = 0.0f;
        if(instance->lifetime > 0.0f) {
            lifetime_pos = instance->age / instance->lifetime;
        }

        switch(particle->animation_mode) {
            case PARTICLE_ANIMATION_RANDOM_FRAME: {
                if(instance->frame < 0) {
                    instance->frame = rand() % particle->spritesheet_frames;
                }
                break;
            }
            case PARTICLE_ANIMATION_ONCE: {
                instance->frame = (int)(lifetime_pos * frame_count * anim_speed);
                if(instance->frame >= frame_count) {
                    instance->frame = frame_count - 1;
                }
                break;
            }
            default: {
                if(particle->spritesheet_duration > 0.0f) {
                    float time_in_cycle = fmodf(instance->age / anim_speed, particle->spritesheet_duration);
                    float cycle_pos = time_in_cycle / particle->spritesheet_duration;
                    instance->frame = (int)fmodf(cycle_pos * frame_count, frame_count);
                } else {
                    instance->frame = (int)fmodf(lifetime_pos * frame_count / anim_speed, frame_count);
                }
                break;
            }
        }
    } else {
        instance->frame = 0;
    }
}

void update_particle(particle_t* particle, float delta) {
    for(int i = 0; i < particle->emitter_count; i++) {
        particle->emitters[i].timer += delta;
        while(particle->emitters[i].timer >= particle->emitters[i].interval) {
            particle->emitters[i].timer -= particle->emitters[i].interval;
            spawn_particle_instance(particle, &particle->emitters[i], particle->emitters[i].interval);
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
