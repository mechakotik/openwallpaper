#version 450

layout(location = 0) in vec2 a_uv;
layout(location = 1) in vec3 a_position;
layout(location = 2) in float a_rotation;
layout(location = 3) in float a_size;
layout(location = 4) in vec4 a_color;
layout(location = 5) in float a_frame;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) out float v_frame;

layout(std140, set = 1, binding = 0) uniform uniforms_t {
    mat4 mvp;
};

void main() {
    vec2 off = a_uv - 0.5;

    float c = cos(a_rotation);
    float s = sin(a_rotation);
    vec2 rotated = vec2(
        off.x * c - off.y * s,
        off.x * s + off.y * c
    );

    vec3 billboard = a_position + vec3(rotated * a_size, 0.0);
    gl_Position = mvp * vec4(billboard, 1.0);

    v_uv = a_uv;
    v_color = a_color;
    v_frame = a_frame;
}

