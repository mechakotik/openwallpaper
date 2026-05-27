#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

static ow_blend_mode blend_additive = {
    .enabled = true,
    .src_color_factor = OW_BLENDFACTOR_SRC_ALPHA,
    .dst_color_factor = OW_BLENDFACTOR_ONE,
    .color_operator = OW_BLENDOP_ADD,
    .src_alpha_factor = OW_BLENDFACTOR_SRC_ALPHA,
    .dst_alpha_factor = OW_BLENDFACTOR_ONE,
    .alpha_operator = OW_BLENDOP_ADD,
};

static ow_blend_mode blend_translucent = {
    .enabled = true,
    .src_color_factor = OW_BLENDFACTOR_SRC_ALPHA,
    .dst_color_factor = OW_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    .color_operator = OW_BLENDOP_ADD,
    .src_alpha_factor = OW_BLENDFACTOR_SRC_ALPHA,
    .dst_alpha_factor = OW_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    .alpha_operator = OW_BLENDOP_ADD,
};

static ow_blend_mode blend_normal = {
    .enabled = true,
    .src_color_factor = OW_BLENDFACTOR_ONE,
    .dst_color_factor = OW_BLENDFACTOR_ZERO,
    .color_operator = OW_BLENDOP_ADD,
    .src_alpha_factor = OW_BLENDFACTOR_ONE,
    .dst_alpha_factor = OW_BLENDFACTOR_ZERO,
    .alpha_operator = OW_BLENDOP_ADD,
};

static ow_blend_mode blend_disabled = {
    .enabled = false,
};

static ow_vertex_attribute vertex_quad_attributes[2] = {
    {.slot = 0, .location = 0, .type = OW_ATTRIBUTE_FLOAT3, .offset = offsetof(wpe_vertex_quad_data, position)},
    {.slot = 0, .location = 1, .type = OW_ATTRIBUTE_FLOAT2, .offset = offsetof(wpe_vertex_quad_data, texcoord)},
};

static wpe_vertex_quad_data vertex_quad[4] = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
    {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
};

static ow_vertex_buffer_id vertex_quad_buffer;
static ow_sampler_id linear_clamp_sampler;
static ow_sampler_id linear_repeat_sampler;
static ow_sampler_id nearest_clamp_sampler;
static ow_sampler_id nearest_repeat_sampler;
static wpe_shader* passthrough_shader;
static ow_pipeline_id passthrough_normal_pipeline;
static ow_pipeline_id passthrough_translucent_pipeline;
static ow_pipeline_id passthrough_additive_pipeline;
static ow_pipeline_id passthrough_disabled_pipeline;
static ow_pipeline_id passthrough_normal_texture_pipeline;
static ow_pipeline_id passthrough_translucent_texture_pipeline;
static ow_pipeline_id passthrough_additive_texture_pipeline;
static ow_pipeline_id passthrough_disabled_texture_pipeline;

bool wpe_starts_with(const char* value, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    return strncmp(value, prefix, prefix_len) == 0;
}

bool wpe_ends_with(const char* value, const char* suffix) {
    size_t value_len = strlen(value);
    size_t suffix_len = strlen(suffix);
    if(value_len < suffix_len) {
        return false;
    }
    return strcmp(value + value_len - suffix_len, suffix) == 0;
}

int wpe_texture_slot_from_uniform_name(const char* name) {
    if(!wpe_starts_with(name, "g_Texture")) {
        return -1;
    }

    int slot = 0;
    const char* cursor = name + strlen("g_Texture");
    if(*cursor < '0' || *cursor > '9') {
        return -1;
    }
    while(*cursor >= '0' && *cursor <= '9') {
        slot = slot * 10 + (*cursor - '0');
        cursor++;
    }
    return slot;
}

int wpe_texture_slot_for_sampler(wpe_sampler_info* sampler, int fallback) {
    if(sampler == NULL) {
        return fallback;
    }
    if(sampler->texture_slot >= 0) {
        return sampler->texture_slot;
    }

    int texture_slot = wpe_texture_slot_from_uniform_name(sampler->name);
    return texture_slot < 0 ? fallback : texture_slot;
}

wpe_texture* wpe_find_texture(int id) {
    if(id < 0) {
        return NULL;
    }
    for(size_t i = 0; i < scene.num_textures; i++) {
        if(scene.textures[i].id == id) {
            return &scene.textures[i];
        }
    }
    return NULL;
}

wpe_texture* wpe_find_texture_by_name(const char* name) {
    if(name == NULL || name[0] == '\0') {
        return NULL;
    }
    for(size_t i = 0; i < scene.num_textures; i++) {
        if(scene.textures[i].name != NULL && strcmp(scene.textures[i].name, name) == 0) {
            return &scene.textures[i];
        }
    }
    return NULL;
}

wpe_shader* wpe_find_shader(int id) {
    if(id < 0) {
        return NULL;
    }
    for(size_t i = 0; i < scene.num_shaders; i++) {
        if(scene.shaders[i].id == id) {
            return &scene.shaders[i];
        }
    }
    return NULL;
}

wpe_object* wpe_find_object(int id) {
    if(id < 0) {
        return NULL;
    }
    for(size_t i = 0; i < scene.num_objects; i++) {
        if(scene.objects[i].id == id) {
            return &scene.objects[i];
        }
    }
    return NULL;
}

static bool texture_name_is_previous(const char* name) {
    return name == NULL || name[0] == '\0' || strcmp(name, "previous") == 0 ||
           wpe_starts_with(name, "_rt_imageLayerComposite");
}

static bool texture_name_is_full_frame(const char* name) {
    return name != NULL && strcmp(name, "_rt_FullFrameBuffer") == 0;
}

static wpe_texture* resolve_texture_ref(int texture_id, const char* name) {
    wpe_texture* texture = wpe_find_texture(texture_id);
    if(texture == NULL) {
        texture = wpe_find_texture_by_name(name);
    }
    return texture;
}

void wpe_resolve_object_parents() {
    for(size_t i = 0; i < scene.num_objects; i++) {
        wpe_object* object = &scene.objects[i];
        object->parent_object = wpe_find_object(object->parent);
        if(object->parent_object == object) {
            object->parent_object = NULL;
        }
    }
}

wpe_texture* wpe_material_texture_at(wpe_material* material, int slot) {
    if(slot < 0 || slot >= material->num_textures || material->textures == NULL) {
        return NULL;
    }
    return material->textures[slot].texture;
}

static ow_blend_mode blend_mode_from_name(const char* name) {
    if(name == NULL || name[0] == '\0' || strcmp(name, "normal") == 0) {
        return blend_normal;
    }
    if(strcmp(name, "translucent") == 0 || strcmp(name, "alpha") == 0) {
        return blend_translucent;
    }
    if(strcmp(name, "additive") == 0 || strcmp(name, "add") == 0) {
        return blend_additive;
    }
    if(strcmp(name, "disabled") == 0 || strcmp(name, "none") == 0) {
        return blend_disabled;
    }
    return blend_translucent;
}

static ow_sampler_id sampler_for_texture(wpe_texture* texture) {
    if(texture == NULL) {
        return linear_clamp_sampler;
    }
    if(texture->interpolation) {
        return texture->clamp_uv ? linear_clamp_sampler : linear_repeat_sampler;
    }
    return texture->clamp_uv ? nearest_clamp_sampler : nearest_repeat_sampler;
}

static ow_pipeline_id create_quad_pipeline(
    wpe_shader* shader, ow_blend_mode blend_mode, ow_texture_format color_target_format) {
    if(shader == NULL || shader->vertex_shader.id == 0 || shader->fragment_shader.id == 0) {
        return (ow_pipeline_id){0};
    }

    return ow_create_pipeline(&(ow_pipeline_info){
        .vertex_bindings =
            &(ow_vertex_binding_info){
                .slot = 0,
                .stride = sizeof(wpe_vertex_quad_data),
            },
        .vertex_bindings_count = 1,
        .vertex_attributes = vertex_quad_attributes,
        .vertex_attributes_count = 2,
        .vertex_shader = shader->vertex_shader,
        .fragment_shader = shader->fragment_shader,
        .color_target_format = color_target_format,
        .blend_mode = blend_mode,
        .depth_test_mode = OW_DEPTHTEST_DISABLED,
        .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
        .cull_mode = OW_CULL_NONE,
    });
}

ow_pipeline_id wpe_passthrough_pipeline_for_blending(const char* blending, bool texture_target) {
    if(blending != NULL && (strcmp(blending, "translucent") == 0 || strcmp(blending, "alpha") == 0)) {
        return texture_target ? passthrough_translucent_texture_pipeline : passthrough_translucent_pipeline;
    }
    if(blending != NULL && (strcmp(blending, "additive") == 0 || strcmp(blending, "add") == 0)) {
        return texture_target ? passthrough_additive_texture_pipeline : passthrough_additive_pipeline;
    }
    if(blending != NULL && (strcmp(blending, "disabled") == 0 || strcmp(blending, "none") == 0)) {
        return texture_target ? passthrough_disabled_texture_pipeline : passthrough_disabled_pipeline;
    }
    return texture_target ? passthrough_normal_texture_pipeline : passthrough_normal_pipeline;
}

void wpe_init_texture(wpe_texture* texture) {
    if(texture == NULL || texture->texture.id != 0) {
        return;
    }

    char path[64];
    (void)snprintf(path, sizeof(path), "textures/%d.webp", texture->id);
    texture->texture = ow_create_texture_from_image(path, &(ow_texture_info){
                                                              .format = OW_TEXTURE_RGBA8_UNORM,
                                                          });
}

void wpe_init_shader(wpe_shader* shader) {
    if(shader == NULL || shader->vertex_shader.id != 0 || shader->fragment_shader.id != 0) {
        return;
    }

    char path[64];
    (void)snprintf(path, sizeof(path), "shaders/%d_vertex.spv", shader->id);
    shader->vertex_shader = ow_create_vertex_shader_from_file(path);
    (void)snprintf(path, sizeof(path), "shaders/%d_fragment.spv", shader->id);
    shader->fragment_shader = ow_create_fragment_shader_from_file(path);

    for(int i = 0; i < shader->num_samplers; i++) {
        shader->samplers[i].default_texture_ref = wpe_find_texture_by_name(shader->samplers[i].default_texture);
    }
}

static void resolve_material_texture_refs(wpe_material_texture* textures, int num_textures, wpe_image_effect* effect) {
    for(int i = 0; i < num_textures; i++) {
        textures[i].texture = resolve_texture_ref(textures[i].texture_id, textures[i].name);
        textures[i].fbo = wpe_find_effect_fbo(effect, textures[i].name);
    }
}

static int max_texture_slot_for_shader(wpe_shader* shader) {
    int max_slot = -1;
    if(shader == NULL) {
        return max_slot;
    }
    for(int i = 0; i < shader->num_samplers; i++) {
        int slot = wpe_texture_slot_for_sampler(&shader->samplers[i], i);
        if(slot > max_slot) {
            max_slot = slot;
        }
    }
    return max_slot;
}

void wpe_init_material(wpe_material* material) {
    material->shader = wpe_find_shader(material->shader_id);
    resolve_material_texture_refs(material->textures, material->num_textures, NULL);
    material->max_texture_slot = material->num_textures - 1;
    if(material->shader == NULL) {
        return;
    }
    wpe_init_shader(material->shader);
    int shader_max_texture_slot = max_texture_slot_for_shader(material->shader);
    if(shader_max_texture_slot > material->max_texture_slot) {
        material->max_texture_slot = shader_max_texture_slot;
    }

    material->texture_pipeline =
        create_quad_pipeline(material->shader, blend_mode_from_name(material->blending), OW_TEXTURE_RGBA8_UNORM);
    material->disabled_texture_pipeline =
        create_quad_pipeline(material->shader, blend_disabled, OW_TEXTURE_RGBA8_UNORM);
}

void wpe_init_material_pass(wpe_material_pass* pass, wpe_image_effect* effect) {
    if(pass == NULL) {
        return;
    }

    resolve_material_texture_refs(pass->textures, pass->num_textures, effect);
    pass->target_fbo = wpe_find_effect_fbo(effect, pass->target);
    pass->max_texture_slot = pass->num_textures - 1;
    for(int i = 0; i < pass->num_binds; i++) {
        pass->binds[i].texture = resolve_texture_ref(-1, pass->binds[i].name);
        pass->binds[i].fbo = wpe_find_effect_fbo(effect, pass->binds[i].name);
        if(pass->binds[i].index > pass->max_texture_slot) {
            pass->max_texture_slot = pass->binds[i].index;
        }
    }
}

void wpe_init_effect_present_pipeline(wpe_material* material, const char* blending) {
    if(material == NULL || material->shader == NULL) {
        return;
    }
    ow_blend_mode blend_mode = blend_mode_from_name(blending);
    material->present_texture_pipeline = create_quad_pipeline(material->shader, blend_mode, OW_TEXTURE_RGBA8_UNORM);
}

void wpe_ensure_texture_size(
    ow_texture_id* texture, uint32_t* current_width, uint32_t* current_height, uint32_t width, uint32_t height) {
    if(width == 0) {
        width = 1;
    }
    if(height == 0) {
        height = 1;
    }
    if(texture->id != 0 && *current_width == width && *current_height == height) {
        return;
    }
    if(texture->id != 0) {
        ow_free_texture(*texture);
    }
    *texture = ow_create_texture(&(ow_texture_info){
        .width = width,
        .height = height,
        .format = OW_TEXTURE_RGBA8_UNORM,
        .render_target = true,
        .mip_levels = 1,
    });
    *current_width = width;
    *current_height = height;
}

static wpe_texture_target texture_target_from_texture(wpe_texture* texture) {
    if(texture == NULL) {
        return (wpe_texture_target){0};
    }
    wpe_init_texture(texture);
    return (wpe_texture_target){
        .texture = texture->texture,
        .width = texture->width > 0 ? (uint32_t)texture->width : 1,
        .height = texture->height > 0 ? (uint32_t)texture->height : 1,
        .source_texture = texture,
    };
}

static wpe_texture_target texture_target_from_material_ref(wpe_material_texture* ref) {
    if(ref == NULL) {
        return (wpe_texture_target){0};
    }
    if(ref->fbo != NULL) {
        return (wpe_texture_target){
            .texture = ref->fbo->texture,
            .width = ref->fbo->width,
            .height = ref->fbo->height,
        };
    }
    return texture_target_from_texture(ref->texture);
}

wpe_effect_fbo* wpe_find_effect_fbo(wpe_image_effect* effect, const char* name) {
    if(effect == NULL || name == NULL || name[0] == '\0') {
        return NULL;
    }
    for(int i = 0; i < effect->num_fbos; i++) {
        if(effect->fbos[i].name != NULL && strcmp(effect->fbos[i].name, name) == 0) {
            return &effect->fbos[i];
        }
    }
    return NULL;
}

static wpe_texture_target texture_target_for_material_ref(wpe_material_texture* ref, wpe_texture_target previous) {
    if(ref == NULL) {
        return (wpe_texture_target){0};
    }
    if(ref->texture != NULL || ref->fbo != NULL) {
        return texture_target_from_material_ref(ref);
    }
    if(texture_name_is_previous(ref->name)) {
        return previous;
    }
    if(texture_name_is_full_frame(ref->name)) {
        return wpe_current_screen_snapshot();
    }
    return texture_target_from_material_ref(ref);
}

static wpe_texture_target texture_target_for_bind(wpe_material_pass_bind* bind, wpe_texture_target previous) {
    if(bind == NULL) {
        return (wpe_texture_target){0};
    }
    if(bind->texture != NULL) {
        return texture_target_from_texture(bind->texture);
    }
    if(bind->fbo != NULL) {
        return (wpe_texture_target){
            .texture = bind->fbo->texture,
            .width = bind->fbo->width,
            .height = bind->fbo->height,
        };
    }
    if(texture_name_is_previous(bind->name)) {
        return previous;
    }
    if(texture_name_is_full_frame(bind->name)) {
        return wpe_current_screen_snapshot();
    }
    return (wpe_texture_target){0};
}

static ow_texture_binding* build_texture_bindings(wpe_material* material, wpe_material_pass* pass,
    wpe_texture_target previous, bool default_previous, wpe_texture_target** texture_slots_out,
    int* num_texture_slots_out, int* num_bindings_out) {
    *texture_slots_out = NULL;
    *num_texture_slots_out = 0;
    *num_bindings_out = 0;

    wpe_shader* shader = material == NULL ? NULL : material->shader;
    if(shader == NULL || shader->num_samplers == 0) {
        return NULL;
    }

    int max_slot = material == NULL ? -1 : material->max_texture_slot;
    if(pass != NULL && pass->max_texture_slot > max_slot) {
        max_slot = pass->max_texture_slot;
    }
    int num_texture_slots = max_slot + 1;
    wpe_texture_target* texture_slots = NULL;
    if(num_texture_slots > 0) {
        texture_slots = calloc((size_t)num_texture_slots, sizeof(wpe_texture_target));
        if(material != NULL) {
            for(int slot = 0; slot < material->num_textures && slot < num_texture_slots; slot++) {
                texture_slots[slot] = texture_target_for_material_ref(&material->textures[slot], previous);
            }
        }
        if(pass != NULL) {
            for(int slot = 0; slot < pass->num_textures && slot < num_texture_slots; slot++) {
                if(pass->textures[slot].name != NULL && pass->textures[slot].name[0] != '\0') {
                    texture_slots[slot] = texture_target_for_material_ref(&pass->textures[slot], previous);
                }
            }
            for(int i = 0; i < pass->num_binds; i++) {
                int slot = pass->binds[i].index;
                if(slot >= 0 && slot < num_texture_slots) {
                    texture_slots[slot] = texture_target_for_bind(&pass->binds[i], previous);
                }
            }
        }
    }

    ow_texture_binding* bindings = calloc((size_t)shader->num_samplers, sizeof(ow_texture_binding));
    for(int sampler_idx = 0; sampler_idx < shader->num_samplers; sampler_idx++) {
        wpe_sampler_info* sampler = &shader->samplers[sampler_idx];
        int texture_slot = wpe_texture_slot_for_sampler(sampler, sampler_idx);

        wpe_texture_target target = {0};
        if(texture_slot >= 0 && texture_slot < num_texture_slots) {
            target = texture_slots[texture_slot];
        }
        if(target.texture.id == 0 && sampler->default_texture != NULL && sampler->default_texture[0] != '\0') {
            target = texture_target_from_texture(sampler->default_texture_ref);
            if(target.texture.id == 0 && texture_name_is_previous(sampler->default_texture)) {
                target = previous;
            } else if(target.texture.id == 0 && texture_name_is_full_frame(sampler->default_texture)) {
                target = wpe_current_screen_snapshot();
            }
            if(texture_slot >= 0 && texture_slot < num_texture_slots) {
                texture_slots[texture_slot] = target;
            }
        }
        if(target.texture.id == 0 && default_previous) {
            target = previous;
            if(texture_slot >= 0 && texture_slot < num_texture_slots) {
                texture_slots[texture_slot] = target;
            }
        }
        if(target.texture.id == 0 && num_texture_slots > 0 && texture_slots[0].texture.id != 0) {
            target = texture_slots[0];
        }

        bindings[sampler_idx] = (ow_texture_binding){
            .slot = (uint32_t)sampler_idx,
            .texture = target.texture,
            .sampler = linear_clamp_sampler,
        };

        if(target.source_texture != NULL) {
            bindings[sampler_idx].sampler = sampler_for_texture(target.source_texture);
        }
    }

    *texture_slots_out = texture_slots;
    *num_texture_slots_out = num_texture_slots;
    *num_bindings_out = shader->num_samplers;
    return bindings;
}

static bool bindings_are_complete(ow_texture_binding* bindings, int num_bindings) {
    for(int i = 0; i < num_bindings; i++) {
        if(bindings[i].texture.id == 0 || bindings[i].sampler.id == 0) {
            return false;
        }
    }
    return true;
}

bool wpe_render_material_to_target(wpe_object* object, wpe_material* material, wpe_material_pass* pass,
    ow_pipeline_id pipeline, ow_texture_id color_target, bool clear, wpe_texture_target previous, bool default_previous,
    wpe_transform_matrices matrices, const wpe_renderer_state* state) {
    if(material == NULL || material->shader == NULL || pipeline.id == 0) {
        return false;
    }

    wpe_texture_target* texture_slots = NULL;
    int num_texture_slots = 0;
    int num_bindings = 0;
    ow_texture_binding* bindings = build_texture_bindings(
        material, pass, previous, default_previous, &texture_slots, &num_texture_slots, &num_bindings);
    if(!bindings_are_complete(bindings, num_bindings)) {
        free(bindings);
        free(texture_slots);
        return false;
    }

    ow_begin_render_pass(&(ow_render_pass_info){
        .color_target = color_target,
        .clear_color = clear,
        .clear_color_rgba = {0.0f, 0.0f, 0.0f, 1.0f},
    });

    wpe_uniform_constant* constants = pass == NULL ? NULL : pass->constants;
    int num_constants = pass == NULL ? 0 : pass->num_constants;

    int vertex_uniform_size = 0;
    uint8_t* vertex_uniform_data =
        wpe_build_uniform_data(material->shader->vertex_uniforms, material->shader->num_vertex_uniforms, object,
            texture_slots, num_texture_slots, constants, num_constants, matrices, state, &vertex_uniform_size);
    if(vertex_uniform_data != NULL && vertex_uniform_size > 0) {
        ow_push_vertex_uniform_data(0, vertex_uniform_data, (uint32_t)vertex_uniform_size);
        free(vertex_uniform_data);
    }

    int fragment_uniform_size = 0;
    uint8_t* fragment_uniform_data =
        wpe_build_uniform_data(material->shader->fragment_uniforms, material->shader->num_fragment_uniforms, object,
            texture_slots, num_texture_slots, constants, num_constants, matrices, state, &fragment_uniform_size);
    if(fragment_uniform_data != NULL && fragment_uniform_size > 0) {
        ow_push_fragment_uniform_data(0, fragment_uniform_data, (uint32_t)fragment_uniform_size);
        free(fragment_uniform_data);
    }

    ow_render_geometry(pipeline,
        &(ow_bindings_info){
            .vertex_buffers = &vertex_quad_buffer,
            .vertex_buffers_count = 1,
            .texture_bindings = bindings,
            .texture_bindings_count = (uint32_t)num_bindings,
        },
        0, 4, 1);

    ow_end_render_pass();
    free(bindings);
    free(texture_slots);
    return true;
}

bool wpe_render_texture_with_passthrough(wpe_object* object, wpe_texture_target source, ow_pipeline_id pipeline,
    ow_texture_id color_target, bool clear, wpe_transform_matrices matrices, const wpe_renderer_state* state) {
    if(passthrough_shader == NULL || pipeline.id == 0 || source.texture.id == 0) {
        return false;
    }

    wpe_texture_target texture_slots[1] = {source};
    ow_texture_binding binding = {
        .slot = 0,
        .texture = source.texture,
        .sampler = linear_clamp_sampler,
    };

    ow_begin_render_pass(&(ow_render_pass_info){
        .color_target = color_target,
        .clear_color = clear,
        .clear_color_rgba = {0.0f, 0.0f, 0.0f, 1.0f},
    });

    int vertex_uniform_size = 0;
    uint8_t* vertex_uniform_data =
        wpe_build_uniform_data(passthrough_shader->vertex_uniforms, passthrough_shader->num_vertex_uniforms, object,
            texture_slots, 1, NULL, 0, matrices, state, &vertex_uniform_size);
    if(vertex_uniform_data != NULL && vertex_uniform_size > 0) {
        ow_push_vertex_uniform_data(0, vertex_uniform_data, (uint32_t)vertex_uniform_size);
        free(vertex_uniform_data);
    }

    int fragment_uniform_size = 0;
    uint8_t* fragment_uniform_data =
        wpe_build_uniform_data(passthrough_shader->fragment_uniforms, passthrough_shader->num_fragment_uniforms, object,
            texture_slots, 1, NULL, 0, matrices, state, &fragment_uniform_size);
    if(fragment_uniform_data != NULL && fragment_uniform_size > 0) {
        ow_push_fragment_uniform_data(0, fragment_uniform_data, (uint32_t)fragment_uniform_size);
        free(fragment_uniform_data);
    }

    ow_render_geometry(pipeline,
        &(ow_bindings_info){
            .vertex_buffers = &vertex_quad_buffer,
            .vertex_buffers_count = 1,
            .texture_bindings = &binding,
            .texture_bindings_count = 1,
        },
        0, 4, 1);

    ow_end_render_pass();
    return true;
}

void wpe_renderer_common_init() {
    vertex_quad_buffer = ow_create_vertex_buffer(sizeof(vertex_quad));
    ow_update_vertex_buffer(vertex_quad_buffer, 0, vertex_quad, sizeof(vertex_quad));

    for(size_t texture_idx = 0; texture_idx < scene.num_textures; texture_idx++) {
        wpe_init_texture(&scene.textures[texture_idx]);
    }

    linear_clamp_sampler = ow_create_sampler(&(ow_sampler_info){
        .min_filter = OW_FILTER_LINEAR,
        .mag_filter = OW_FILTER_LINEAR,
        .mip_filter = OW_FILTER_LINEAR,
        .wrap_x = OW_WRAP_CLAMP,
        .wrap_y = OW_WRAP_CLAMP,
    });
    linear_repeat_sampler = ow_create_sampler(&(ow_sampler_info){
        .min_filter = OW_FILTER_LINEAR,
        .mag_filter = OW_FILTER_LINEAR,
        .mip_filter = OW_FILTER_LINEAR,
        .wrap_x = OW_WRAP_REPEAT,
        .wrap_y = OW_WRAP_REPEAT,
    });
    nearest_clamp_sampler = ow_create_sampler(&(ow_sampler_info){
        .min_filter = OW_FILTER_NEAREST,
        .mag_filter = OW_FILTER_NEAREST,
        .mip_filter = OW_FILTER_NEAREST,
        .wrap_x = OW_WRAP_CLAMP,
        .wrap_y = OW_WRAP_CLAMP,
    });
    nearest_repeat_sampler = ow_create_sampler(&(ow_sampler_info){
        .min_filter = OW_FILTER_NEAREST,
        .mag_filter = OW_FILTER_NEAREST,
        .mip_filter = OW_FILTER_NEAREST,
        .wrap_x = OW_WRAP_REPEAT,
        .wrap_y = OW_WRAP_REPEAT,
    });

    passthrough_shader = wpe_find_shader(scene.passthrough_shader_id);
    if(passthrough_shader != NULL) {
        wpe_init_shader(passthrough_shader);
        passthrough_normal_pipeline = create_quad_pipeline(passthrough_shader, blend_normal, OW_TEXTURE_SWAPCHAIN);
        passthrough_translucent_pipeline =
            create_quad_pipeline(passthrough_shader, blend_translucent, OW_TEXTURE_SWAPCHAIN);
        passthrough_additive_pipeline = create_quad_pipeline(passthrough_shader, blend_additive, OW_TEXTURE_SWAPCHAIN);
        passthrough_disabled_pipeline = create_quad_pipeline(passthrough_shader, blend_disabled, OW_TEXTURE_SWAPCHAIN);
        passthrough_normal_texture_pipeline =
            create_quad_pipeline(passthrough_shader, blend_normal, OW_TEXTURE_RGBA8_UNORM);
        passthrough_translucent_texture_pipeline =
            create_quad_pipeline(passthrough_shader, blend_translucent, OW_TEXTURE_RGBA8_UNORM);
        passthrough_additive_texture_pipeline =
            create_quad_pipeline(passthrough_shader, blend_additive, OW_TEXTURE_RGBA8_UNORM);
        passthrough_disabled_texture_pipeline =
            create_quad_pipeline(passthrough_shader, blend_disabled, OW_TEXTURE_RGBA8_UNORM);
    }
}

bool wpe_passthrough_available() {
    return passthrough_shader != NULL;
}
