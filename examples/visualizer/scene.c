#include "openwallpaper.h"

#define NUM_BARS 128

typedef struct vertex_t {
    float x, y;
} vertex_t;

ow_vertex_buffer_id vertex_buffer;
ow_pipeline_id pipeline;

vertex_t vertices[NUM_BARS * 6];

void generate_vertices() {
    float spectrum[NUM_BARS];
    ow_get_audio_spectrum(spectrum, NUM_BARS);
    int tp = 0;

    for(int i = 0; i < NUM_BARS; i++) {
        float bottom_y = -0.8f;
        float top_y = bottom_y + 1.6f * spectrum[i];
        float bar_width = 1.6f / (float)NUM_BARS;
        float left_x = -0.8f + (1.6f - bar_width) * ((float)i / (float)(NUM_BARS - 1));
        float right_x = left_x + bar_width;
        vertices[tp++] = (vertex_t){left_x, bottom_y};
        vertices[tp++] = (vertex_t){left_x, top_y};
        vertices[tp++] = (vertex_t){right_x, bottom_y};
        vertices[tp++] = (vertex_t){left_x, top_y};
        vertices[tp++] = (vertex_t){right_x, bottom_y};
        vertices[tp++] = (vertex_t){right_x, top_y};
    }

    ow_begin_copy_pass();
    ow_update_vertex_buffer(vertex_buffer, 0, vertices, sizeof(vertices));
    ow_end_copy_pass();
}

__attribute__((export_name("init"))) void init() {
    vertex_buffer = ow_create_vertex_buffer(sizeof(vertices));
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
            },
        .vertex_attributes_count = 1,
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .topology = OW_TOPOLOGY_TRIANGLES,
    });
}

__attribute__((export_name("update"))) void update(float delta) {
    generate_vertices();

    ow_bindings_info bindings = {
        .vertex_buffers = &vertex_buffer,
        .vertex_buffers_count = 1,
    };

    ow_begin_render_pass(&(ow_render_pass_info){
        .clear_color = true,
        .clear_color_rgba = {0, 0, 0, 1},
    });
    ow_render_geometry(pipeline, &bindings, 0, sizeof(vertices) / sizeof(vertex_t), 1);
    ow_end_render_pass();
}
