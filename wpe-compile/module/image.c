#include <string.h>
#include "defs.h"

static uint32_t object_target_width(wpe_object* object, const wpe_renderer_state* state) {
    if(object->image.fullscreen) {
        return state->screen_width == 0 ? 1 : state->screen_width;
    }
    if(object->size.w > 0.0f) {
        return (uint32_t)(object->size.w + 0.5f);
    }
    wpe_texture* texture = wpe_material_texture_at(&object->image.material, 0);
    if(texture != NULL && texture->width > 0) {
        return (uint32_t)texture->width;
    }
    return state->screen_width == 0 ? 1 : state->screen_width;
}

static uint32_t object_target_height(wpe_object* object, const wpe_renderer_state* state) {
    if(object->image.fullscreen) {
        return state->screen_height == 0 ? 1 : state->screen_height;
    }
    if(object->size.h > 0.0f) {
        return (uint32_t)(object->size.h + 0.5f);
    }
    wpe_texture* texture = wpe_material_texture_at(&object->image.material, 0);
    if(texture != NULL && texture->height > 0) {
        return (uint32_t)texture->height;
    }
    return state->screen_height == 0 ? 1 : state->screen_height;
}

static void ensure_image_effect_targets(wpe_object* object, const wpe_renderer_state* state) {
    uint32_t width = object_target_width(object, state);
    uint32_t height = object_target_height(object, state);
    bool ping_pong_size_changed = object->image.ping_pong_width != width || object->image.ping_pong_height != height;
    if(ping_pong_size_changed && object->image.ping_pong[1].id != 0) {
        ow_free_texture(object->image.ping_pong[1]);
        object->image.ping_pong[1] = (ow_texture_id){0};
    }

    wpe_ensure_texture_size(
        &object->image.ping_pong[0], &object->image.ping_pong_width, &object->image.ping_pong_height, width, height);
    uint32_t unused_width = object->image.ping_pong[1].id == 0 ? 0 : object->image.ping_pong_width;
    uint32_t unused_height = object->image.ping_pong[1].id == 0 ? 0 : object->image.ping_pong_height;
    wpe_ensure_texture_size(&object->image.ping_pong[1], &unused_width, &unused_height, width, height);

    for(int effect_idx = 0; effect_idx < object->image.num_effects; effect_idx++) {
        wpe_image_effect* effect = &object->image.effects[effect_idx];
        for(int fbo_idx = 0; fbo_idx < effect->num_fbos; fbo_idx++) {
            wpe_effect_fbo* fbo = &effect->fbos[fbo_idx];
            int scale = fbo->scale == 0 ? 1 : fbo->scale;
            uint32_t fbo_width = width / (uint32_t)scale;
            uint32_t fbo_height = height / (uint32_t)scale;
            wpe_ensure_texture_size(&fbo->texture, &fbo->width, &fbo->height, fbo_width, fbo_height);
        }
    }
}

static wpe_texture_target image_ping_pong_target(wpe_object* object, int index) {
    return (wpe_texture_target){
        .texture = object->image.ping_pong[index],
        .width = object->image.ping_pong_width,
        .height = object->image.ping_pong_height,
    };
}

static bool is_final_present_pass(wpe_object* object, int effect_index, int pass_index) {
    for(int scan_effect_idx = effect_index; scan_effect_idx < object->image.num_effects; scan_effect_idx++) {
        wpe_image_effect* scan_effect = &object->image.effects[scan_effect_idx];
        int scan_pass_count =
            scan_effect->num_passes < scan_effect->num_materials ? scan_effect->num_passes : scan_effect->num_materials;
        int scan_pass_idx = scan_effect_idx == effect_index ? pass_index + 1 : 0;
        for(; scan_pass_idx < scan_pass_count; scan_pass_idx++) {
            if(scan_effect->passes[scan_pass_idx].target_fbo == NULL) {
                return false;
            }
        }
    }
    return true;
}

static bool image_has_puppet_mesh(wpe_object* object) {
    return object->image.puppet != NULL && object->image.puppet->num_meshes > 0 && object->image.puppet->meshes != NULL;
}

static bool puppet_mesh_is_renderable(wpe_puppet_mesh* mesh) {
    if(mesh == NULL || mesh->material_name == NULL) {
        return true;
    }
    if(strstr(mesh->material_name, "channelmap") != NULL) {
        return false;
    }
    return true;
}

static bool render_puppet_material_to_target(wpe_object* object, wpe_material* material, ow_pipeline_id pipeline,
    ow_texture_id color_target, bool clear, wpe_texture_target previous, bool default_previous,
    wpe_transform_matrices matrices, const wpe_renderer_state* state) {
    if(!image_has_puppet_mesh(object)) {
        return false;
    }

    bool rendered = false;
    for(int mesh_idx = 0; mesh_idx < object->image.puppet->num_meshes; mesh_idx++) {
        wpe_puppet_mesh* mesh = &object->image.puppet->meshes[mesh_idx];
        if(!puppet_mesh_is_renderable(mesh)) {
            continue;
        }
        if(!wpe_render_material_mesh_to_target(object, material, NULL, pipeline, mesh, color_target, clear && !rendered,
               previous, default_previous, matrices, state)) {
            return false;
        }
        rendered = true;
    }
    return rendered;
}

static wpe_effect_render_result render_image_effects(
    wpe_object* object, const wpe_renderer_state* state, bool stop_at_final_present) {
    wpe_effect_render_result result = {
        .previous = image_ping_pong_target(object, 0),
    };
    int write_idx = 1;

    for(int effect_idx = 0; effect_idx < object->image.num_effects; effect_idx++) {
        wpe_image_effect* effect = &object->image.effects[effect_idx];
        int pass_count = effect->num_passes < effect->num_materials ? effect->num_passes : effect->num_materials;
        for(int pass_idx = 0; pass_idx < pass_count; pass_idx++) {
            wpe_material* material = &effect->materials[pass_idx];
            wpe_material_pass* pass = &effect->passes[pass_idx];
            wpe_texture_target target = image_ping_pong_target(object, write_idx);
            wpe_effect_fbo* fbo = pass->target_fbo;
            if(stop_at_final_present && fbo == NULL && pass->final_present) {
                result.final_effect = effect;
                result.final_pass = pass;
                result.final_material = material;
                result.has_final_pass = true;
                return result;
            }

            int next_write_idx = write_idx;
            bool updates_previous = false;
            if(fbo != NULL) {
                target = (wpe_texture_target){
                    .texture = fbo->texture,
                    .width = fbo->width,
                    .height = fbo->height,
                };
            } else {
                next_write_idx = 1 - write_idx;
                updates_previous = true;
            }

            if(wpe_render_material_to_target(object, material, pass, material->disabled_texture_pipeline,
                   target.texture, true, result.previous, true, wpe_default_transform_matrices(), state) &&
                updates_previous) {
                result.previous = target;
                write_idx = next_write_idx;
            }
        }
    }

    return result;
}

void wpe_renderer_init_image_object(wpe_object* obj) {
    if(obj->type != OBJECTTYPE_IMAGE) {
        return;
    }
    wpe_init_material(&obj->image.material);
    if(obj->image.puppet != NULL) {
        wpe_init_puppet_model(obj->image.puppet);
        if(image_has_puppet_mesh(obj) && obj->image.num_effects > 0) {
            wpe_init_material(&obj->image.puppet_material);
        }
    }
    for(int effect_idx = 0; effect_idx < obj->image.num_effects; effect_idx++) {
        wpe_image_effect* effect = &obj->image.effects[effect_idx];
        for(int pass_idx = 0; pass_idx < effect->num_passes; pass_idx++) {
            wpe_init_material_pass(&effect->passes[pass_idx], effect);
        }
        for(int material_idx = 0; material_idx < effect->num_materials; material_idx++) {
            wpe_init_material(&effect->materials[material_idx]);
            wpe_init_effect_present_pipeline(&effect->materials[material_idx], obj->image.material.blending);
        }
    }
    for(int effect_idx = 0; effect_idx < obj->image.num_effects; effect_idx++) {
        wpe_image_effect* effect = &obj->image.effects[effect_idx];
        int pass_count = effect->num_passes < effect->num_materials ? effect->num_passes : effect->num_materials;
        for(int pass_idx = 0; pass_idx < pass_count; pass_idx++) {
            effect->passes[pass_idx].final_present =
                effect->passes[pass_idx].target_fbo == NULL && is_final_present_pass(obj, effect_idx, pass_idx);
        }
    }
}

bool wpe_renderer_render_image_object(wpe_object* object, bool clear, const wpe_renderer_state* state) {
    wpe_material* material = &object->image.material;
    bool has_puppet_mesh = image_has_puppet_mesh(object);
    ow_pipeline_id base_pipeline = has_puppet_mesh ? material->mesh_pipeline : material->texture_pipeline;
    if(base_pipeline.id == 0 || material->shader == NULL || !wpe_screen_snapshot_available()) {
        return false;
    }

    wpe_transform_matrices matrices =
        object->image.fullscreen ? wpe_default_transform_matrices()
                                 : wpe_compute_transform_matrices(wpe_transform_parameters_for_object(object, state));
    wpe_transform_matrices mesh_matrices =
        object->image.fullscreen
            ? wpe_default_transform_matrices()
            : wpe_compute_transform_matrices(wpe_transform_parameters_for_object_mesh(object, state));
    wpe_texture_target screen_previous = wpe_current_screen_snapshot();

    if(object->image.num_effects == 0) {
        bool default_previous = object->image.passthrough || material->num_textures == 0;
        wpe_texture_target snapshot_target = wpe_next_screen_snapshot();
        bool prepared = wpe_prepare_next_screen_snapshot(object, clear, state);
        bool rendered = false;
        if(prepared && has_puppet_mesh) {
            rendered = render_puppet_material_to_target(object, material, material->mesh_pipeline,
                snapshot_target.texture, clear, screen_previous, default_previous, mesh_matrices, state);
        } else if(prepared) {
            rendered = wpe_render_material_to_target(object, material, NULL, material->texture_pipeline,
                snapshot_target.texture, clear, screen_previous, default_previous, matrices, state);
        }
        if(rendered) {
            wpe_commit_screen_snapshot();
            return true;
        }
        return false;
    }

    if(!wpe_passthrough_available()) {
        return false;
    }

    ensure_image_effect_targets(object, state);
    wpe_texture_target first_target = image_ping_pong_target(object, 0);
    bool default_previous = object->image.passthrough || material->num_textures == 0;
    if(!wpe_render_material_to_target(object, material, NULL, material->disabled_texture_pipeline, first_target.texture,
           true, screen_previous, default_previous, wpe_default_transform_matrices(), state)) {
        return false;
    }

    wpe_effect_render_result effect_result = render_image_effects(object, state, !has_puppet_mesh);
    wpe_texture_target snapshot_target = wpe_next_screen_snapshot();
    bool rendered_snapshot = false;
    if(wpe_prepare_next_screen_snapshot(object, clear, state)) {
        if(has_puppet_mesh) {
            wpe_material* puppet_material = &object->image.puppet_material;
            rendered_snapshot =
                render_puppet_material_to_target(object, puppet_material, puppet_material->mesh_pipeline,
                    snapshot_target.texture, clear, effect_result.previous, true, mesh_matrices, state);
        } else if(effect_result.has_final_pass) {
            rendered_snapshot = wpe_render_material_to_target(object, effect_result.final_material,
                effect_result.final_pass, effect_result.final_material->present_texture_pipeline,
                snapshot_target.texture, clear, effect_result.previous, true, matrices, state);
        } else {
            ow_pipeline_id final_texture_pipeline = wpe_passthrough_pipeline_for_blending(material->blending, true);
            rendered_snapshot = wpe_render_texture_with_passthrough(object, effect_result.previous,
                final_texture_pipeline, snapshot_target.texture, clear, matrices, state);
        }
    }
    if(rendered_snapshot) {
        wpe_commit_screen_snapshot();
    }
    return rendered_snapshot;
}
