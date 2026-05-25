#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "openwallpaper.h"

typedef struct {
    float position[3];
    float texcoord[2];
} VertexQuadData;

ow_vertex_attribute vertex_quad_attributes[2] = {
    {.slot = 0, .location = 0, .type = OW_ATTRIBUTE_FLOAT3, .offset = 0},
    {.slot = 0, .location = 1, .type = OW_ATTRIBUTE_FLOAT2, .offset = sizeof(float) * 3},
};

VertexQuadData vertex_quad[4] = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
    {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
};

void init_material(Material* mat) {
    // TODO: cache textures and shaders
    char path[32];

    if(mat->num_textures != 0) {
        mat->textures = malloc(sizeof(ow_texture_id) * mat->num_textures);
        for(int i = 0; i < mat->num_textures; i++) {
            (void)snprintf(path, sizeof(path), "textures/%d.webp", mat->texture_ids[i]);
            mat->textures[i] = ow_create_texture_from_image(path, &(ow_texture_info){});
        }
    }

    (void)snprintf(path, sizeof(path), "shaders/%d_vertex.spv", mat->shader_id);
    mat->vertex_shader = ow_create_vertex_shader_from_file(path);
    (void)snprintf(path, sizeof(path), "shaders/%d_fragment.spv", mat->shader_id);
    mat->fragment_shader = ow_create_fragment_shader_from_file(path);

    mat->pipeline = ow_create_pipeline(&(ow_pipeline_info){
        .vertex_bindings =
            &(ow_vertex_binding_info){
                .slot = 0,
                .stride = sizeof(VertexQuadData),
            },
        .vertex_bindings_count = 1,
        .vertex_attributes = vertex_quad_attributes,
        .vertex_attributes_count = 2,
        .vertex_shader = mat->vertex_shader,
        .fragment_shader = mat->fragment_shader,
        .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
    });
}

void init_image_object(Object* obj) {
    init_material(&obj->image.material);
}

void init_object(Object* obj) {
    switch(obj->type) {
        case OBJECTTYPE_IMAGE:
            init_image_object(obj);
            break;
        default:
            break;
    }
}

void update_image_object(Object* obj, float delta) {}

void update_object(Object* obj, float delta) {
    switch(obj->type) {
        case OBJECTTYPE_IMAGE:
            update_image_object(obj, delta);
            break;
        default:
            break;
    }
}

__attribute__((export_name("init"))) void init() {
    for(int i = 0; i < scene.num_objects; i++) {
        init_object(&scene.objects[i]);
    }
}

__attribute__((export_name("update"))) void update(float delta) {
    for(int i = 0; i < scene.num_objects; i++) {
        update_object(&scene.objects[i], delta);
    }
}
