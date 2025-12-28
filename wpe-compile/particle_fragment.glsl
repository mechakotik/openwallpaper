#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in flat int v_frame;

layout(location = 0) out vec4 f_color;

layout(set = 2, binding = 0) uniform sampler2D u_texture;

layout(std140, set = 3, binding = 0) uniform uniforms_t {
    ivec2 spritesheet_size;
};

void main() {
    vec2 uv = v_uv;

    if(spritesheet_size.x > 0) {
        int cols = spritesheet_size.x;
        int rows = spritesheet_size.y;
        int frame_count = cols * rows;

        int frame_x = v_frame % cols;
        int frame_y = v_frame / cols;

        float frame_width = 1.0 / float(cols);
        float frame_height = 1.0 / float(rows);

        uv = vec2(
            (float(frame_x) + v_uv.x) * frame_width,
            (float(frame_y) + v_uv.y) * frame_height
        );
    }

    vec4 tex_color = texture(u_texture, uv);
    f_color = tex_color * v_color;
}

