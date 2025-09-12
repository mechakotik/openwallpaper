#include "openwallpaper.h"

typedef struct vertex_t {
    float x, y, z;
    float r, g, b, a;
} vertex_t;

static vertex_t vertices[] = {
    {0, 0.5, 0, 1, 0, 0, 1},
    {-0.5, -0.5, 0, 0, 1, 0, 1},
    {0.5, -0.5, 0, 0, 0, 1, 1},
};

static struct {
    ow_id vertex_buffer;
    ow_id pipeline;
    float color_shift;
} state = {0};

__attribute__((export_name("init"))) void init() {
    state.vertex_buffer = ow_create_buffer(OW_BUFFER_VERTEX, sizeof(vertices));
    ow_id vertex_shader = ow_create_shader_from_file("vertex.spv", OW_SHADER_VERTEX);
    ow_id fragment_shader = ow_create_shader_from_file("fragment.spv", OW_SHADER_FRAGMENT);

    ow_vertex_binding_info vertex_binding_info = {0};
    vertex_binding_info.slot = 0;
    vertex_binding_info.stride = sizeof(vertex_t);

    ow_vertex_attribute vertex_attributes[2];
    vertex_attributes[0].slot = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].type = OW_ATTRIBUTE_FLOAT3;
    vertex_attributes[0].offset = 0;
    vertex_attributes[1].slot = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].type = OW_ATTRIBUTE_FLOAT4;
    vertex_attributes[1].offset = sizeof(float) * 3;

    ow_pipeline_info pipeline_info = {0};
    pipeline_info.vertex_bindings = &vertex_binding_info;
    pipeline_info.vertex_bindings_count = 1;
    pipeline_info.vertex_attributes = vertex_attributes;
    pipeline_info.vertex_attributes_count = 2;
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.topology = OW_TOPOLOGY_TRIANGLES;
    state.pipeline = ow_create_pipeline(&pipeline_info);

    ow_begin_copy_pass();
    ow_update_buffer(state.vertex_buffer, 0, vertices, sizeof(vertices));
    ow_end_copy_pass();
}

__attribute__((export_name("update"))) void update(float delta) {
    state.color_shift += delta * 0.25;
    state.color_shift -= (int)state.color_shift;

    ow_vertex_buffer_binding vertex_buffer_binding = {.buffer = state.vertex_buffer, .offset = 0};
    ow_bindings_info bindings = {0};
    bindings.vertex_buffer_bindings = &vertex_buffer_binding;
    bindings.vertex_buffer_bindings_count = 1;

    ow_pass_info pass_info = {0};
    pass_info.clear_color = true;
    pass_info.clear_color_rgba[0] = 0;
    pass_info.clear_color_rgba[1] = 0;
    pass_info.clear_color_rgba[2] = 0;
    pass_info.clear_color_rgba[3] = 1;

    ow_begin_render_pass(&pass_info);
    ow_push_uniform_data(OW_SHADER_VERTEX, 0, &state.color_shift, sizeof(state.color_shift));
    ow_render_geometry(state.pipeline, &bindings, 0, 3, 1);
    ow_end_render_pass();
}
