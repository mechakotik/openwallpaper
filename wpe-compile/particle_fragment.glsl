#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in float v_frame;

layout(location = 0) out vec4 f_color;

layout(set = 2, binding = 0) uniform sampler2D u_texture;

layout(std140, set = 3, binding = 0) uniform uniforms_t {
    vec2 spritesheet_size;
};

void main() {
    vec2 uv = v_uv;

    if(spritesheet_size.x > 0.0) {
        float cols = spritesheet_size.x;
        float rows = spritesheet_size.y;
        
        float frame_idx = floor(v_frame);
        float frame_x = mod(frame_idx, cols);
        float frame_y = floor(frame_idx / cols);

        float frame_width = 1.0 / cols;
        float frame_height = 1.0 / rows;

        uv = vec2(
            (frame_x + v_uv.x) * frame_width,
            (frame_y + v_uv.y) * frame_height
        );
    }

    vec4 tex_color = texture(u_texture, uv);
    f_color = tex_color * v_color;
}

