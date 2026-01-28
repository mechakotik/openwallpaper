#include <stdlib.h>
#include "openwallpaper.h"

#define NUM_INSTANCES 200

typedef struct {
    float x, y;
    float u, v;
} vertex_t;

typedef struct {
    float x, y;
    float opacity;
} instance_t;

static vertex_t vertices[] = {
    {0, 0, 0, 1},
    {0, 0.3, 0, 0},
    {0.003, 0, 1, 1},
    {0.003, 0.3, 1, 0},
};

ow_vertex_buffer_id vertex_buffer;
ow_vertex_buffer_id instance_buffer;
ow_texture_id texture;
ow_sampler_id sampler;
ow_pipeline_id pipeline;

instance_t instance_data[NUM_INSTANCES];

void init_instance_data() {
    for(int i = 0; i < NUM_INSTANCES; i++) {
        instance_data[i].x = ((float)rand() / (float)RAND_MAX) * 2 - 1;
        instance_data[i].y = ((float)rand() / (float)RAND_MAX) * 2.3 - 1;
        instance_data[i].opacity = (float)rand() / (float)RAND_MAX * 0.5;
    }
}

void update_instance_data(float delta) {
    for(int i = 0; i < NUM_INSTANCES; i++) {
        instance_data[i].y -= 5 * delta;
        if(instance_data[i].y < -1.3) {
            instance_data[i].x = ((float)rand() / (float)RAND_MAX) * 2 - 1;
            instance_data[i].y += 2.3;
            instance_data[i].opacity = (float)rand() / (float)RAND_MAX * 0.5;
        }
    }
}

__attribute__((export_name("init"))) void init() {
    init_instance_data();
    vertex_buffer = ow_create_vertex_buffer(sizeof(vertices));
    instance_buffer = ow_create_vertex_buffer(sizeof(instance_data));

    ow_begin_copy_pass();
    ow_update_vertex_buffer(vertex_buffer, 0, vertices, sizeof(vertices));
    ow_update_vertex_buffer(instance_buffer, 0, instance_data, sizeof(instance_data));
    texture = ow_create_texture_from_image("dot.png", &(ow_texture_info){
        .format = OW_TEXTURE_RGBA8_UNORM,
    });
    ow_end_copy_pass();

    sampler = ow_create_sampler(&(ow_sampler_info){
        .min_filter = OW_FILTER_LINEAR,
        .mag_filter = OW_FILTER_LINEAR,
        .mip_filter = OW_FILTER_LINEAR,
        .wrap_x = OW_WRAP_CLAMP,
        .wrap_y = OW_WRAP_CLAMP,
    });

    ow_vertex_shader_id vertex_shader = ow_create_vertex_shader_from_file("vertex.spv");
    ow_fragment_shader_id fragment_shader = ow_create_fragment_shader_from_file("fragment.spv");

    pipeline = ow_create_pipeline(&(ow_pipeline_info){
        .vertex_bindings = (ow_vertex_binding_info[]){
            {.slot = 0, .stride = sizeof(vertex_t)},
            {.slot = 1, .stride = sizeof(instance_t), .per_instance = true},
        },
        .vertex_bindings_count = 2,
        .vertex_attributes = (ow_vertex_attribute[]){
            {.location = 0, .slot = 0, .type = OW_ATTRIBUTE_FLOAT2, .offset = 0},
            {.location = 1, .slot = 0, .type = OW_ATTRIBUTE_FLOAT2, .offset = sizeof(float) * 2},
            {.location = 2, .slot = 1, .type = OW_ATTRIBUTE_FLOAT2, .offset = 0},
            {.location = 3, .slot = 1, .type = OW_ATTRIBUTE_FLOAT, .offset = sizeof(float) * 2},
        },
        .vertex_attributes_count = 4,
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
    });
}

__attribute__((export_name("update"))) void update(float delta) {
    update_instance_data(delta);
    ow_begin_copy_pass();
    ow_update_vertex_buffer(instance_buffer, 0, instance_data, sizeof(instance_data));
    ow_end_copy_pass();

    ow_bindings_info bindings = {
        .vertex_buffers = (ow_vertex_buffer_id[]){
            vertex_buffer,
            instance_buffer,
        },
        .vertex_buffers_count = 2,
        .texture_bindings = &(ow_texture_binding){
            .texture = texture,
            .sampler = sampler,
        },
        .texture_bindings_count = 1,
    };

    ow_begin_render_pass(&(ow_render_pass_info){
        .clear_color = true,
        .clear_color_rgba = {0, 0, 0, 1},
    });

    ow_render_geometry(pipeline, &bindings, 0, 4, NUM_INSTANCES);
    ow_end_render_pass();
}
