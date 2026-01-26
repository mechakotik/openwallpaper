#version 460

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec2 a_origin;
layout(location = 3) in float a_opacity;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out float v_opacity;

void main() {
    gl_Position = vec4(a_origin + a_position, 0, 1);
    v_uv = a_uv;
    v_opacity = a_opacity;
}
