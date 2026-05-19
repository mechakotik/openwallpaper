#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include "doomgeneric.h"
#include "openwallpaper.h"

typedef struct vertex_t {
    float x, y;
    float u, v;
} vertex_t;

static vertex_t vertices[] = {
    {-1, -1, 0, 1},
    {-1, 1, 0, 0},
    {1, -1, 1, 1},
    {1, 1, 1, 0},
};

ow_vertex_buffer_id vertex_buffer;
ow_texture_id texture;
ow_sampler_id sampler;
ow_pipeline_id pipeline;

float time_s = 0.0f;

int system(const char* cmd) {
    return -1;
}

__attribute__((export_name("init"))) void init() {
    vertex_buffer = ow_create_vertex_buffer(sizeof(vertices));

    texture = ow_create_texture(&(ow_texture_info){
        .width = DOOMGENERIC_RESX,
        .height = DOOMGENERIC_RESY,
        .format = OW_TEXTURE_RGBA8_UNORM,
    });

    ow_begin_copy_pass();
    ow_update_vertex_buffer(vertex_buffer, 0, vertices, sizeof(vertices));
    ow_end_copy_pass();

    sampler = ow_create_sampler(&(ow_sampler_info){
        .min_filter = OW_FILTER_NEAREST,
        .mag_filter = OW_FILTER_NEAREST,
        .mip_filter = OW_FILTER_NEAREST,
        .wrap_x = OW_WRAP_CLAMP,
        .wrap_y = OW_WRAP_CLAMP,
    });

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
                {.slot = 0, .location = 1, .type = OW_ATTRIBUTE_FLOAT2, .offset = sizeof(float) * 2},
            },
        .vertex_attributes_count = 2,
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
    });

    doomgeneric_Create(0, NULL);
}

__attribute__((export_name("update"))) void update(float delta) {
    // printf("update\n");

    time_s += delta;
    doomgeneric_Tick();

    ow_begin_copy_pass();
    ow_update_texture(DG_ScreenBuffer, DOOMGENERIC_RESX,
        &(ow_texture_update_destination){
            .texture = texture,
            .x = 0,
            .y = 0,
            .w = DOOMGENERIC_RESX,
            .h = DOOMGENERIC_RESY,
        });
    ow_end_copy_pass();

    ow_bindings_info bindings = {
        .vertex_buffers = &vertex_buffer,
        .vertex_buffers_count = 1,
        .texture_bindings =
            &(ow_texture_binding){
                .texture = texture,
                .sampler = sampler,
            },
        .texture_bindings_count = 1,
    };

    ow_begin_render_pass(&(ow_render_pass_info){0});
    ow_render_geometry(pipeline, &bindings, 0, 4, 1);
    ow_end_render_pass();
}

void DG_Init() {}

void DG_DrawFrame() {
    // printf("DG_DrawFrame\n");
}

void DG_SleepMs(uint32_t ms) {
    // printf("DG_SleepMs %i ms\n", ms);

    time_s += 0.001 * (float)ms;
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs() {
    return (uint32_t)(time_s * 1000.0);
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    return 0;
}

void DG_SetWindowTitle(const char* title) {}
