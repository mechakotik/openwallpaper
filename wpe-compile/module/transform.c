#include <math.h>
#include "defs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

wpe_mat4 wpe_mat4_identity() {
    wpe_mat4 res = {0};
    res.at[0][0] = 1.0f;
    res.at[1][1] = 1.0f;
    res.at[2][2] = 1.0f;
    res.at[3][3] = 1.0f;
    return res;
}

static wpe_mat4 mat4_scale_xy(float sx, float sy) {
    wpe_mat4 m = wpe_mat4_identity();
    m.at[0][0] = sx;
    m.at[1][1] = sy;
    return m;
}

static wpe_mat4 mat4_scale_xyz(float sx, float sy, float sz) {
    wpe_mat4 m = wpe_mat4_identity();
    m.at[0][0] = sx;
    m.at[1][1] = sy;
    m.at[2][2] = sz;
    return m;
}

static wpe_mat4 mat4_translate_xyz(float tx, float ty, float tz) {
    wpe_mat4 m = wpe_mat4_identity();
    m.at[3][0] = tx;
    m.at[3][1] = ty;
    m.at[3][2] = tz;
    return m;
}

static wpe_mat4 mat4_multiply(wpe_mat4 a, wpe_mat4 b) {
    wpe_mat4 res;
    for(int col = 0; col < 4; col++) {
        for(int row = 0; row < 4; row++) {
            res.at[col][row] = 0.0f;
            for(int k = 0; k < 4; k++) {
                res.at[col][row] += a.at[k][row] * b.at[col][k];
            }
        }
    }
    return res;
}

static wpe_mat3 mat3_identity() {
    wpe_mat3 res = {0};
    res.at[0][0] = 1.0f;
    res.at[1][1] = 1.0f;
    res.at[2][2] = 1.0f;
    return res;
}

static wpe_mat3 mat3_from_mat4(wpe_mat4 m) {
    wpe_mat3 res = {0};
    for(int col = 0; col < 3; col++) {
        for(int row = 0; row < 3; row++) {
            res.at[col][row] = m.at[col][row];
        }
    }
    return res;
}

static wpe_vec3 mat3_multiply(wpe_mat3 m, wpe_vec3 v) {
    return (wpe_vec3){{
        m.at[0][0] * v.at[0] + m.at[1][0] * v.at[1] + m.at[2][0] * v.at[2],
        m.at[0][1] * v.at[0] + m.at[1][1] * v.at[1] + m.at[2][1] * v.at[2],
        m.at[0][2] * v.at[0] + m.at[1][2] * v.at[1] + m.at[2][2] * v.at[2],
    }};
}

static wpe_mat3 mat3_inverse(wpe_mat3 m) {
    float m00 = m.at[0][0];
    float m01 = m.at[1][0];
    float m02 = m.at[2][0];
    float m10 = m.at[0][1];
    float m11 = m.at[1][1];
    float m12 = m.at[2][1];
    float m20 = m.at[0][2];
    float m21 = m.at[1][2];
    float m22 = m.at[2][2];

    float c00 = m11 * m22 - m12 * m21;
    float c01 = m12 * m20 - m10 * m22;
    float c02 = m10 * m21 - m11 * m20;
    float c10 = m02 * m21 - m01 * m22;
    float c11 = m00 * m22 - m02 * m20;
    float c12 = m01 * m20 - m00 * m21;
    float c20 = m01 * m12 - m02 * m11;
    float c21 = m02 * m10 - m00 * m12;
    float c22 = m00 * m11 - m01 * m10;

    float det = m00 * c00 + m01 * c01 + m02 * c02;
    if(fabsf(det) < 1e-7f) {
        return mat3_identity();
    }

    float inv_det = 1.0f / det;
    wpe_mat3 inv = {0};
    inv.at[0][0] = c00 * inv_det;
    inv.at[1][0] = c10 * inv_det;
    inv.at[2][0] = c20 * inv_det;
    inv.at[0][1] = c01 * inv_det;
    inv.at[1][1] = c11 * inv_det;
    inv.at[2][1] = c21 * inv_det;
    inv.at[0][2] = c02 * inv_det;
    inv.at[1][2] = c12 * inv_det;
    inv.at[2][2] = c22 * inv_det;
    return inv;
}

static wpe_vec4 vec4_from_vec3(wpe_vec3 v) {
    return (wpe_vec4){{v.at[0], v.at[1], v.at[2], 0.0f}};
}

static wpe_vec3 vec3_sub(wpe_vec3 a, wpe_vec3 b) {
    return (wpe_vec3){{a.at[0] - b.at[0], a.at[1] - b.at[1], a.at[2] - b.at[2]}};
}

static wpe_vec3 vec3_scale(wpe_vec3 v, float s) {
    return (wpe_vec3){{v.at[0] * s, v.at[1] * s, v.at[2] * s}};
}

static wpe_vec3 vec3_cross(wpe_vec3 a, wpe_vec3 b) {
    return (wpe_vec3){{
        a.at[1] * b.at[2] - a.at[2] * b.at[1],
        a.at[2] * b.at[0] - a.at[0] * b.at[2],
        a.at[0] * b.at[1] - a.at[1] * b.at[0],
    }};
}

static float vec3_dot(wpe_vec3 a, wpe_vec3 b) {
    return a.at[0] * b.at[0] + a.at[1] * b.at[1] + a.at[2] * b.at[2];
}

static wpe_vec3 vec3_normalize(wpe_vec3 v) {
    float len = sqrtf(vec3_dot(v, v));
    if(len > 1e-5f) {
        float inv_len = 1.0f / len;
        v.at[0] *= inv_len;
        v.at[1] *= inv_len;
        v.at[2] *= inv_len;
    }
    return v;
}

static float wrap_angle(float angle) {
    return remainderf(angle, 2.0f * (float)M_PI);
}

static wpe_vec3 mat4_transform_point(wpe_mat4 m, wpe_vec3 v) {
    return (wpe_vec3){{
        m.at[0][0] * v.x + m.at[1][0] * v.y + m.at[2][0] * v.z + m.at[3][0],
        m.at[0][1] * v.x + m.at[1][1] * v.y + m.at[2][1] * v.z + m.at[3][1],
        m.at[0][2] * v.x + m.at[1][2] * v.y + m.at[2][2] * v.z + m.at[3][2],
    }};
}

static wpe_mat4 mat4_rotation_xyz(float angle_x, float angle_y, float angle_z) {
    float rx = wrap_angle(angle_x);
    float ry = wrap_angle(angle_y);
    float rz = wrap_angle(angle_z);

    float cx = cosf(rx);
    float sx = sinf(rx);
    float cy = cosf(ry);
    float sy = sinf(ry);
    float cz = cosf(rz);
    float sz = sinf(rz);

    wpe_mat4 rot_x = wpe_mat4_identity();
    rot_x.at[1][1] = cx;
    rot_x.at[1][2] = sx;
    rot_x.at[2][1] = -sx;
    rot_x.at[2][2] = cx;

    wpe_mat4 rot_y = wpe_mat4_identity();
    rot_y.at[0][0] = cy;
    rot_y.at[0][2] = -sy;
    rot_y.at[2][0] = sy;
    rot_y.at[2][2] = cy;

    wpe_mat4 rot_z = wpe_mat4_identity();
    rot_z.at[0][0] = cz;
    rot_z.at[0][1] = sz;
    rot_z.at[1][0] = -sz;
    rot_z.at[1][1] = cz;

    return mat4_multiply(rot_z, mat4_multiply(rot_y, rot_x));
}

static wpe_mat4 mat4_transform(
    float tx, float ty, float tz, float sx, float sy, float sz, float angle_x, float angle_y, float angle_z) {
    wpe_mat4 translation = mat4_translate_xyz(tx, ty, tz);
    wpe_mat4 rotation = mat4_rotation_xyz(angle_x, angle_y, angle_z);
    wpe_mat4 scale = mat4_scale_xyz(sx, sy, sz);
    return mat4_multiply(translation, mat4_multiply(rotation, scale));
}

static wpe_mat4 object_local_transform(wpe_object* object) {
    return mat4_transform(object->origin.x, object->origin.y, object->origin.z, object->scale.x, object->scale.y,
        object->scale.z, object->angles.x, object->angles.y, object->angles.z);
}

static wpe_mat4 object_global_transform(wpe_object* object, wpe_object* root, int depth) {
    if(object == NULL) {
        return wpe_mat4_identity();
    }
    if(depth >= (int)scene.num_objects) {
        return object_local_transform(object);
    }

    wpe_object* parent = object->parent_object;
    if(parent == NULL || parent == root) {
        return object_local_transform(object);
    }
    return mat4_multiply(object_global_transform(parent, root, depth + 1), object_local_transform(object));
}

static wpe_mat4 object_parent_transform(wpe_object* object) {
    wpe_object* parent = object->parent_object;
    if(parent == NULL || parent == object) {
        return wpe_mat4_identity();
    }
    return object_global_transform(parent, object, 0);
}

static wpe_mat4 mat4_look_at(wpe_vec3 eye, wpe_vec3 center, wpe_vec3 up) {
    wpe_vec3 f = vec3_normalize(vec3_sub(center, eye));
    wpe_vec3 s = vec3_normalize(vec3_cross(f, up));
    wpe_vec3 u = vec3_cross(s, f);

    wpe_mat4 res = wpe_mat4_identity();
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

static wpe_mat4 mat4_ortho(float left, float right, float bottom, float top, float near_z, float far_z) {
    if(right == left || top == bottom || far_z == near_z) {
        return wpe_mat4_identity();
    }

    wpe_mat4 trans = wpe_mat4_identity();
    trans = mat4_multiply(
        mat4_translate_xyz(-(left + right) * 0.5f, -(top + bottom) * 0.5f, -(near_z + far_z) * 0.5f), trans);
    trans = mat4_multiply(mat4_scale_xyz(2.0f / (right - left), 2.0f / (top - bottom), 2.0f / (far_z - near_z)), trans);
    trans = mat4_multiply(trans, mat4_scale_xyz(1.0f, 1.0f, -1.0f));
    trans = mat4_multiply(mat4_scale_xyz(1.0f, 1.0f, 0.5f), trans);
    trans = mat4_multiply(mat4_translate_xyz(0.0f, 0.0f, 0.5f), trans);
    return trans;
}

static wpe_mat4 mat4_perspective(float fov_radians, float aspect, float near_z, float far_z) {
    if(fov_radians == 0.0f || aspect == 0.0f) {
        return wpe_mat4_identity();
    }
    if(near_z <= 0.0f) {
        near_z = 0.01f;
    }
    if(far_z <= near_z) {
        far_z = near_z + 1.0f;
    }

    wpe_mat4 trans = wpe_mat4_identity();
    trans = mat4_multiply(mat4_scale_xyz(near_z, near_z, near_z + far_z), trans);
    trans.at[2][3] = 1.0f;
    trans.at[3][3] = 0.0f;
    trans.at[3][2] = -near_z * far_z;

    float top = tanf(fov_radians * 0.5f) * fabsf(near_z);
    float right = top * aspect;

    trans = mat4_multiply(trans, mat4_scale_xyz(1.0f, 1.0f, -1.0f));
    trans = mat4_multiply(mat4_scale_xyz(1.0f, 1.0f, -1.0f), trans);
    return mat4_multiply(mat4_ortho(-right, right, -top, top, near_z, far_z), trans);
}

wpe_transform_matrices wpe_default_transform_matrices() {
    wpe_transform_matrices res;
    res.model = wpe_mat4_identity();
    res.view_projection = wpe_mat4_identity();
    res.model_view_projection = wpe_mat4_identity();
    res.orientation_up = (wpe_vec4){{0.0f, 1.0f, 0.0f, 0.0f}};
    res.orientation_right = (wpe_vec4){{1.0f, 0.0f, 0.0f, 0.0f}};
    res.orientation_forward = (wpe_vec4){{0.0f, 0.0f, 1.0f, 0.0f}};
    res.parallax_position_x = 0.5f;
    res.parallax_position_y = 0.5f;
    return res;
}

static wpe_vec3 compute_camera_shake_offset(wpe_transform_parameters params) {
    wpe_vec3 shake = {{0.0f, 0.0f, 0.0f}};
    if(!params.shake_enabled) {
        return shake;
    }

    float roughness = powf(params.shake_roughness, 3.0f);
    float phase = params.shake_speed * params.shake_speed * params.time;
    float amplitude = params.shake_amplitude * 0.1f;

    shake.at[0] = cosf(phase);
    shake.at[1] = sinf(phase * 1.333f);
    shake.at[2] = sinf(phase);

    if(!params.perspective) {
        shake.at[2] = 0.0f;
        amplitude *= params.scene_height * 0.1f;
    }

    if(roughness > 0.001f && roughness != 1.0f) {
        float len_sq = shake.at[0] * shake.at[0] + shake.at[1] * shake.at[1] + shake.at[2] * shake.at[2];
        if(len_sq > 0.0f) {
            float len = sqrtf(len_sq);
            float scale = powf(len, roughness) / len;
            shake.at[0] *= scale;
            shake.at[1] *= scale;
            shake.at[2] *= scale;
        }
    }

    shake.at[0] *= amplitude;
    shake.at[1] *= amplitude;
    shake.at[2] *= amplitude;
    return shake;
}

static wpe_mat4 apply_camera_zoom(wpe_mat4 vp, wpe_transform_parameters params) {
    float camera_zoom = (params.zoom > 0.0f ? params.zoom : 1.0f);
    if(fabsf(camera_zoom - 1.0f) < 1e-6f) {
        return vp;
    }

    wpe_mat4 to_center = mat4_translate_xyz(params.scene_width * 0.5f, params.scene_height * 0.5f, 0.0f);
    wpe_mat4 scale = mat4_scale_xyz(camera_zoom, camera_zoom, 1.0f);
    wpe_mat4 from_center = mat4_translate_xyz(-params.scene_width * 0.5f, -params.scene_height * 0.5f, 0.0f);

    vp = mat4_multiply(vp, to_center);
    vp = mat4_multiply(vp, scale);
    vp = mat4_multiply(vp, from_center);
    return vp;
}

wpe_transform_parameters wpe_transform_parameters_for_object(wpe_object* object, const wpe_renderer_state* state) {
    return (wpe_transform_parameters){
        .scene_width = scene.general.ortho.w,
        .scene_height = scene.general.ortho.h,
        .zoom = scene.general.zoom,
        .origin_x = object->origin.x,
        .origin_y = object->origin.y,
        .origin_z = object->origin.z,
        .size_x = object->size.w,
        .size_y = object->size.h,
        .scale_x = object->scale.x,
        .scale_y = object->scale.y,
        .scale_z = object->scale.z,
        .parent_model = object_parent_transform(object),
        .has_parent_model = true,
        .angle_x = object->angles.x,
        .angle_y = object->angles.y,
        .angle_z = object->angles.z,
        .parallax_depth_x = object->parallax_depth.x,
        .parallax_depth_y = object->parallax_depth.y,
        .parallax_enabled = scene.general.parallax,
        .parallax_amount = scene.general.parallax_amount,
        .parallax_mouse_influence = scene.general.parallax_mouse_influence,
        .perspective = object->perspective,
        .near_z = scene.general.near_z,
        .far_z = scene.general.far_z,
        .fov = scene.general.fov,
        .shake_enabled = scene.general.shake,
        .shake_amplitude = scene.general.shake_amplitude,
        .shake_roughness = scene.general.shake_roughness,
        .shake_speed = scene.general.shake_speed,
        .scale_mode = state->scale_mode,
        .mouse_x = state->parallax_mouse_x,
        .mouse_y = state->parallax_mouse_y,
        .time = state->time_seconds,
    };
}

wpe_transform_matrices wpe_compute_transform_matrices(wpe_transform_parameters params) {
    wpe_transform_matrices res = wpe_default_transform_matrices();

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
    wpe_mat4 vp = wpe_mat4_identity();
    wpe_vec3 camera_shake = compute_camera_shake_offset(params);

    if(params.perspective) {
        float fov_radians = params.fov * (float)M_PI / 180.0f;
        float camera_distance = (params.scene_height * 0.5f) / tanf(fov_radians * 0.5f);
        wpe_vec3 eye = {{
            params.scene_width * 0.5f + camera_shake.at[0],
            params.scene_height * 0.5f + camera_shake.at[1],
            camera_distance + camera_shake.at[2],
        }};
        wpe_vec3 center = {{
            params.scene_width * 0.5f + camera_shake.at[0],
            params.scene_height * 0.5f + camera_shake.at[1],
            camera_shake.at[2],
        }};
        wpe_vec3 up = {{0.0f, 1.0f, 0.0f}};
        wpe_mat4 view = mat4_look_at(eye, center, up);
        wpe_mat4 proj = mat4_perspective(fov_radians, scene_aspect, params.near_z, params.far_z);
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

        wpe_mat4 aspect_fix = mat4_scale_xy(sx, sy);
        wpe_mat4 pv = mat4_multiply(proj, view);
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

        vp.at[3][0] = -(params.scene_width * 0.5f + camera_shake.at[0]) * vp_scale_x;
        vp.at[3][1] = -(params.scene_height * 0.5f + camera_shake.at[1]) * vp_scale_y;
        vp.at[3][2] = 0.0f;
        vp.at[3][3] = 1.0f;
    }

    vp = apply_camera_zoom(vp, params);

    float clamped_parallax_x = params.mouse_x;
    float clamped_parallax_y = params.mouse_y;
    if(clamped_parallax_x < 0.0f) {
        clamped_parallax_x = 0.0f;
    }
    if(clamped_parallax_x > 1.0f) {
        clamped_parallax_x = 1.0f;
    }
    if(clamped_parallax_y < 0.0f) {
        clamped_parallax_y = 0.0f;
    }
    if(clamped_parallax_y > 1.0f) {
        clamped_parallax_y = 1.0f;
    }

    float parallax_pos_x = 0.5f;
    float parallax_pos_y = 0.5f;
    if(params.parallax_enabled) {
        parallax_pos_x = clamped_parallax_x;
        parallax_pos_y = clamped_parallax_y;
    }

    wpe_mat4 parent_model = params.has_parent_model ? params.parent_model : wpe_mat4_identity();
    wpe_vec3 node_pos =
        mat4_transform_point(parent_model, (wpe_vec3){{params.origin_x, params.origin_y, params.origin_z}});
    float cam_pos_x = params.scene_width * 0.5f;
    float cam_pos_y = params.scene_height * 0.5f;
    float parallax_offset_x = 0.0f;
    float parallax_offset_y = 0.0f;

    if(params.parallax_enabled) {
        float parallax_vec_x = 0.5f - parallax_pos_x;
        float parallax_vec_y = 0.5f - parallax_pos_y;
        float depth_x = params.parallax_depth_x;
        float depth_y = params.parallax_depth_y;
        float dx = (node_pos.x - cam_pos_x + parallax_vec_x * params.scene_width) * depth_x * params.parallax_amount;
        float dy = (node_pos.y - cam_pos_y + parallax_vec_y * params.scene_height) * depth_y * params.parallax_amount;
        parallax_offset_x = dx;
        parallax_offset_y = dy;
    }

    float tx = params.origin_x;
    float ty = params.origin_y;
    float tz = params.origin_z;
    float sx = 0.5f * params.size_x * params.scale_x;
    float sy = 0.5f * params.size_y * params.scale_y;
    float sz = params.scale_z;

    wpe_mat4 local_model = mat4_transform(tx, ty, tz, sx, sy, sz, params.angle_x, params.angle_y, params.angle_z);
    wpe_mat4 model = mat4_multiply(parent_model, local_model);
    if(parallax_offset_x != 0.0f || parallax_offset_y != 0.0f) {
        model = mat4_multiply(mat4_translate_xyz(parallax_offset_x, parallax_offset_y, 0.0f), model);
    }

    wpe_mat3 model3 = mat3_from_mat4(model);
    wpe_mat3 inv_model3 = mat3_inverse(model3);

    wpe_vec3 world_x = {{model3.at[0][0], model3.at[0][1], model3.at[0][2]}};
    wpe_vec3 world_y = {{model3.at[1][0], model3.at[1][1], model3.at[1][2]}};
    wpe_vec3 screen_forward = {{0.0f, 0.0f, 1.0f}};
    wpe_vec3 projected_world_x = vec3_sub(world_x, vec3_scale(screen_forward, vec3_dot(world_x, screen_forward)));
    wpe_vec3 projected_world_y = vec3_sub(world_y, vec3_scale(screen_forward, vec3_dot(world_y, screen_forward)));

    wpe_vec3 desired_world_up = projected_world_y;
    if(vec3_dot(desired_world_up, desired_world_up) < 1e-12f) {
        desired_world_up = projected_world_x;
    }
    desired_world_up = vec3_normalize(desired_world_up);

    wpe_vec3 desired_world_right = vec3_cross(desired_world_up, screen_forward);
    if(vec3_dot(desired_world_right, desired_world_right) < 1e-12f) {
        desired_world_right = (wpe_vec3){{1.0f, 0.0f, 0.0f}};
    }
    desired_world_right = vec3_normalize(desired_world_right);

    wpe_vec3 raw_orientation_right = mat3_multiply(inv_model3, desired_world_right);
    wpe_vec3 raw_orientation_up = mat3_multiply(inv_model3, desired_world_up);
    wpe_vec3 orientation_right = vec3_normalize(raw_orientation_right);
    wpe_vec3 orientation_forward = vec3_normalize(vec3_cross(raw_orientation_right, raw_orientation_up));
    wpe_vec3 orientation_screen_forward = vec3_normalize(mat3_multiply(inv_model3, screen_forward));
    wpe_vec3 orientation_up = vec3_normalize(vec3_cross(orientation_screen_forward, orientation_right));

    res.model = model;
    res.view_projection = vp;
    res.model_view_projection = mat4_multiply(vp, model);
    res.orientation_right = vec4_from_vec3(orientation_right);
    res.orientation_up = vec4_from_vec3(orientation_up);
    res.orientation_forward = vec4_from_vec3(orientation_forward);
    res.parallax_position_x = parallax_pos_x;
    res.parallax_position_y = parallax_pos_y;
    return res;
}
