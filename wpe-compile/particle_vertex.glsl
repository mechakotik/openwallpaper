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
};

void main() {
    vec2 off = a_uv - 0.5;
    vec3 local = vec3(off * a_size, 0.0);

    vec3 c = cos(a_rotation);
    vec3 s = sin(a_rotation);

    mat3 rot_z = mat3(
        c.z, -s.z, 0.0,
        s.z,  c.z, 0.0,
        0.0,  0.0, 1.0
    );
    mat3 rot_x = mat3(
        1.0, 0.0, 0.0,
        0.0, c.x, -s.x,
        0.0, s.x,  c.x
    );
    mat3 rot_y = mat3(
         c.y, 0.0, s.y,
         0.0, 1.0, 0.0,
        -s.y, 0.0, c.y
    );

    vec3 rotated = rot_y * (rot_x * (rot_z * local));
    vec3 billboard = a_position + rotated;
    gl_Position = mvp * vec4(billboard, 1.0);

    v_uv = a_uv;
    v_color = a_color;
    v_frame = a_frame;
}

