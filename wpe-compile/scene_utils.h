#ifndef WPE_COMPILE_SCENE_UTILS_H
#define WPE_COMPILE_SCENE_UTILS_H

#include "openwallpaper.h"
#include "openwallpaper_std140.h"

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

#endif
