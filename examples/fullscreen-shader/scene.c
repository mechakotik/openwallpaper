#include "openwallpaper.h"

typedef struct {
    float resolution[2];
    float time;
} uniforms_t;

ow_vertex_shader_id vertex_shader;
ow_fragment_shader_id fragment_shader;
ow_pipeline_id pipeline;
float time = 0;

__attribute__((export_name("init"))) void init() {
    vertex_shader = ow_create_vertex_shader_from_file("vertex.spv");
    fragment_shader = ow_create_fragment_shader_from_file("fragment.spv");

    pipeline = ow_create_pipeline(&(ow_pipeline_info){
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .topology = OW_TOPOLOGY_TRIANGLES,
    });
}

__attribute__((export_name("update"))) void update(float delta) {
    time += delta;
    uint32_t screen_width, screen_height;
    ow_get_screen_size(&screen_width, &screen_height);

    uniforms_t data = {
        .resolution = {(float)screen_width, (float)screen_height},
        .time = time,
    };

    ow_begin_render_pass(&(ow_pass_info){
        .clear_color = true,
        .clear_color_rgba = {0.0f, 0.0f, 0.0f, 1.0f},
    });

    ow_push_fragment_uniform_data(0, &data, sizeof(data));
    ow_bindings_info bindings = {0};
    ow_render_geometry(pipeline, &bindings, 0, 3, 1);

    ow_end_render_pass();
}
