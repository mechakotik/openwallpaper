#include "openwallpaper.h"

typedef struct vertex_t {
    float x, y;
    float r, g, b;
} vertex_t;

static vertex_t vertices[] = {
    {0, 0.5, 1, 0, 0},
    {-0.5, -0.5, 0, 1, 0},
    {0.5, -0.5, 0, 0, 1},
};

ow_vertex_buffer_id vertex_buffer;
ow_pipeline_id pipeline;

__attribute__((export_name("init"))) void init() {
    vertex_buffer = ow_create_vertex_buffer(sizeof(vertices));
    ow_begin_copy_pass();
    ow_update_vertex_buffer(vertex_buffer, 0, vertices, sizeof(vertices));
    ow_end_copy_pass();

    ow_vertex_shader_id vertex_shader = ow_create_vertex_shader_from_file("vertex.spv");
    ow_fragment_shader_id fragment_shader = ow_create_fragment_shader_from_file("fragment.spv");

    pipeline = ow_create_pipeline(&(ow_pipeline_info){
        .vertex_bindings =
            &(ow_vertex_binding_info){
                .slot = 0,
                .stride = sizeof(vertex_t),
            },
        .vertex_bindings_count = 1,
        .vertex_attributes =
            (ow_vertex_attribute[]){
                {.slot = 0, .location = 0, .type = OW_ATTRIBUTE_FLOAT2, .offset = 0},
                {.slot = 0, .location = 1, .type = OW_ATTRIBUTE_FLOAT3, .offset = sizeof(float) * 2},
            },
        .vertex_attributes_count = 2,
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .topology = OW_TOPOLOGY_TRIANGLES,
    });
}

__attribute__((export_name("update"))) void update(float delta) {
    ow_bindings_info bindings = {
        .vertex_buffers = &vertex_buffer,
        .vertex_buffers_count = 1,
    };

    ow_begin_render_pass(&(ow_pass_info){
        .clear_color = true,
        .clear_color_rgba = {0, 0, 0, 1},
    });
    ow_render_geometry(pipeline, &bindings, 0, 3, 1);
    ow_end_render_pass();
}
