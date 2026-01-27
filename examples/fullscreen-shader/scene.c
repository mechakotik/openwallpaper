#include "openwallpaper.h"

typedef struct vertex_t {
    float x, y;
} vertex_t;

static vertex_t vertices[] = {
    {-1, -1},
    {-1, 1},
    {1, -1},
    {1, 1},
};

typedef struct {
    float resolution[2];
    float time;
} uniforms_t;

ow_vertex_buffer_id vertex_buffer;
ow_vertex_shader_id vertex_shader;
ow_fragment_shader_id fragment_shader;
ow_pipeline_id pipeline;
float time = 0;

__attribute__((export_name("init"))) void init() {
    vertex_buffer = ow_create_vertex_buffer(sizeof(vertices));
    ow_begin_copy_pass();
    ow_update_vertex_buffer(vertex_buffer, 0, vertices, sizeof(vertices));
    ow_end_copy_pass();

    vertex_shader = ow_create_vertex_shader_from_file("vertex.spv");
    fragment_shader = ow_create_fragment_shader_from_file("fragment.spv");

    pipeline = ow_create_pipeline(&(ow_pipeline_info){
        .vertex_bindings = &(ow_vertex_binding_info){
            .slot = 0,
            .stride = sizeof(vertex_t),
        },
        .vertex_bindings_count = 1,
        .vertex_attributes = (ow_vertex_attribute[]){
            {.slot = 0, .location = 0, .type = OW_ATTRIBUTE_FLOAT2, .offset = 0},
        },
        .vertex_attributes_count = 1,
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
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

    ow_bindings_info bindings = {
        .vertex_buffers = &vertex_buffer,
        .vertex_buffers_count = 1,
    };

    ow_begin_render_pass(&(ow_render_pass_info){0});
    ow_push_fragment_uniform_data(0, &data, sizeof(data));
    ow_render_geometry(pipeline, &bindings, 0, 4, 1);
    ow_end_render_pass();
}
