#version 450

layout(location = 0) in vec2 a_uv;
layout(location = 1) in vec3 a_position;
layout(location = 2) in vec3 a_rotation;
layout(location = 3) in float a_size;
layout(location = 4) in vec4 a_color;
layout(location = 5) in int a_frame;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) out flat int v_frame;

layout(std140, set = 1, binding = 0) uniform uniforms_t {
    mat4 mvp;
    vec4 orientation_up;
    vec4 orientation_right;
    vec4 orientation_forward;
    float texture_ratio;
};

void compute_particle_tangents(in vec3 rotation, out vec3 right, out vec3 up) {
    vec3 r_cos = cos(rotation);
    vec3 r_sin = sin(rotation);

    mat3 m_rotation = mat3(
        r_cos.y, 0.0, r_sin.y,
        0.0, 1.0, 0.0,
        -r_sin.y, 0.0, r_cos.y
    ) * mat3(
        1.0, 0.0, 0.0,
        0.0, r_cos.x, -r_sin.x,
        0.0, r_sin.x, r_cos.x
    ) * mat3(
        r_cos.z, -r_sin.z, 0.0,
        r_sin.z, r_cos.z, 0.0,
        0.0, 0.0, 1.0
    );

    m_rotation = mat3(orientation_right.xyz, orientation_up.xyz, orientation_forward.xyz) * m_rotation;
    right = m_rotation * vec3(1.0, 0.0, 0.0);
    up = m_rotation * vec3(0.0, 1.0, 0.0);
}

vec3 particle_position(vec2 uvs, float texture_ratio, vec4 position_size, vec3 right, vec3 up) {
    return position_size.xyz + (position_size.w * right * (uvs.x - 0.5) -
        position_size.w * up * (uvs.y - 0.5) * texture_ratio);
}

void main() {
    vec3 right;
    vec3 up;
    compute_particle_tangents(a_rotation, right, up);

    vec3 position = particle_position(a_uv, texture_ratio, vec4(a_position.xyz, a_size), right, up);
    gl_Position = mvp * vec4(position, 1.0);
    v_uv = a_uv;
    v_color = a_color;
    v_frame = a_frame;
}

