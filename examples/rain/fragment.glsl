#version 460

layout(location = 0) in vec2 v_uv;
layout(location = 1) in float v_opacity;

layout(location = 0) out vec4 f_color;

layout(set = 2, binding = 0) uniform sampler2D u_texture;

void main() {
    f_color = texture(u_texture, v_uv) * vec4(v_opacity, v_opacity, v_opacity, 1);
}
