#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include "defs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float texcoord[2];
} wpe_particle_vertex_data;

typedef struct {
    wpe_mat4 mvp;
    wpe_vec4 orientation_up;
    wpe_vec4 orientation_right;
    wpe_vec4 orientation_forward;
    float texture_ratio;
    float padding[3];
} wpe_particle_vertex_uniforms;

typedef struct {
    int32_t spritesheet_size[2];
    float screen_size[2];
} wpe_particle_fragment_uniforms;

static wpe_particle_vertex_data particle_vertex_data[4] = {
    {{0.0f, 0.0f}},
    {{0.0f, 1.0f}},
    {{1.0f, 0.0f}},
    {{1.0f, 1.0f}},
};

static ow_vertex_attribute particle_vertex_attributes[6] = {
    {.slot = 0, .location = 0, .type = OW_ATTRIBUTE_FLOAT2, .offset = offsetof(wpe_particle_vertex_data, texcoord)},
    {.slot = 1, .location = 1, .type = OW_ATTRIBUTE_FLOAT3, .offset = offsetof(wpe_particle_instance_data, position)},
    {.slot = 1, .location = 2, .type = OW_ATTRIBUTE_FLOAT3, .offset = offsetof(wpe_particle_instance_data, rotation)},
    {.slot = 1, .location = 3, .type = OW_ATTRIBUTE_FLOAT, .offset = offsetof(wpe_particle_instance_data, size)},
    {.slot = 1, .location = 4, .type = OW_ATTRIBUTE_FLOAT4, .offset = offsetof(wpe_particle_instance_data, color)},
    {.slot = 1, .location = 5, .type = OW_ATTRIBUTE_INT, .offset = offsetof(wpe_particle_instance_data, frame)},
};

static ow_vertex_buffer_id particle_vertex_buffer;

static float rand_float(float min, float max) {
    return min + (max - min) * (float)rand() / (float)RAND_MAX;
}

static float fade_value(float life, float start, float end, float start_value, float end_value) {
    if(life <= start) {
        return start_value;
    }
    if(life > end) {
        return end_value;
    }
    float span = end - start;
    if(fabsf(span) < 1e-6f) {
        return end_value;
    }
    float pass = (life - start) / span;
    return start_value + (end_value - start_value) * pass;
}

static int particle_spritesheet_frame(wpe_particle_object* particle, wpe_particle_instance* instance) {
    if(particle->spritesheet_frames <= 0) {
        return 0;
    }

    if(particle->random_frame) {
        if(instance->frame < 0) {
            instance->frame = rand() % particle->spritesheet_frames;
        }
        return instance->frame;
    }

    float lifetime_fraction = 0.0f;
    if(instance->lifetime > 0.0f) {
        lifetime_fraction = instance->age / instance->lifetime;
    }

    float frame_position = lifetime_fraction * particle->sequence_multiplier * (float)particle->spritesheet_frames;
    int frame = (int)floorf(frame_position);
    if(particle->spritesheet_frames > 0) {
        frame %= particle->spritesheet_frames;
        if(frame < 0) {
            frame += particle->spritesheet_frames;
        }
    }
    return frame;
}

static void spawn_particle_instance(wpe_particle_object* particle, wpe_particle_emitter* emitter, float duration) {
    (void)duration;
    if(particle->alive_count == particle->max_count) {
        return;
    }
    while(particle->instances[particle->free_pos].alive) {
        particle->free_pos++;
        if(particle->free_pos >= particle->max_count) {
            particle->free_pos = 0;
        }
    }

    wpe_particle_instance* instance = &particle->instances[particle->free_pos];
    *instance = (wpe_particle_instance){0};
    instance->alive = true;
    particle->alive_count++;

    for(int it = 0; it < 10; it++) {
        float dist = 0.0f;
        for(int i = 0; i < 3; i++) {
            float offset = rand_float(emitter->distance_min[i], emitter->distance_max[i]);
            if(fabsf(emitter->distance_max[i]) > 1e-6f) {
                float normalized = offset / emitter->distance_max[i];
                dist += normalized * normalized;
            }
            offset *= emitter->directions[i];
            if(rand() % 2) {
                offset = -offset;
            }
            if((emitter->sign[i] > 0 && offset < 0.0f) || (emitter->sign[i] < 0 && offset > 0.0f)) {
                offset = -offset;
            }
            instance->position[i] = emitter->origin[i] + offset;
        }
        if(dist <= 1.0f) {
            break;
        }
    }

    instance->size = rand_float(particle->init.min_size, particle->init.max_size);
    instance->size /= 2.0f;
    instance->initial_size = instance->size;
    instance->frame = -1;

    float factor = rand_float(0.0f, 1.0f);
    for(int i = 0; i < 3; i++) {
        instance->velocity[i] =
            particle->init.min_velocity[i] + (particle->init.max_velocity[i] - particle->init.min_velocity[i]) * factor;
        instance->rotation[i] = rand_float(particle->init.min_rotation[i], particle->init.max_rotation[i]);
        instance->angular_velocity[i] =
            rand_float(particle->init.min_angular_velocity[i], particle->init.max_angular_velocity[i]);
    }

    instance->lifetime = rand_float(particle->init.min_lifetime, particle->init.max_lifetime);

    factor = rand_float(0.0f, 1.0f);
    for(int i = 0; i < 3; i++) {
        float color =
            particle->init.min_color[i] + (particle->init.max_color[i] - particle->init.min_color[i]) * factor;
        instance->color[i] = color;
        instance->initial_color[i] = color;
    }

    instance->alpha = rand_float(particle->init.min_alpha, particle->init.max_alpha);
    instance->initial_alpha = instance->alpha;
    instance->age = 0.0f;
}

static void update_particle_instance(wpe_particle_object* particle, wpe_particle_instance* instance, float delta) {
    if(!instance->alive) {
        return;
    }
    instance->age += delta;
    if(instance->age >= instance->lifetime) {
        instance->alive = false;
        particle->alive_count--;
        return;
    }

    if(particle->operator.movement.enabled) {
        float movement_speed = particle->operator.movement.speed;
        if(fabsf(movement_speed) < 0.0001f) {
            movement_speed = 1.0f;
        }
        float drag_coeff = -2.0f * particle->operator.movement.drag;
        for(int i = 0; i < 3; i++) {
            float acceleration = particle->operator.movement.gravity[i] + drag_coeff * instance->velocity[i];
            instance->velocity[i] += acceleration * movement_speed * delta;
            instance->position[i] += instance->velocity[i] * delta;
        }
    }

    if(particle->operator.angular_movement.enabled) {
        for(int i = 0; i < 3; i++) {
            float acceleration = -particle->operator.angular_movement.drag * instance->angular_velocity[i] +
                                 particle->operator.angular_movement.force[i];
            instance->angular_velocity[i] += acceleration * delta;
        }
        const float pi = (float)M_PI;
        const float two_pi = (float)(2.0 * M_PI);
        for(int i = 0; i < 3; i++) {
            instance->rotation[i] += instance->angular_velocity[i] * delta;
            while(instance->rotation[i] > pi) {
                instance->rotation[i] -= two_pi;
            }
            while(instance->rotation[i] < -pi) {
                instance->rotation[i] += two_pi;
            }
        }
    }

    if(particle->operator.size_change.enabled && instance->lifetime > 0.0f) {
        float life = instance->age / instance->lifetime;
        float multiplier =
            fade_value(life, particle->operator.size_change.start_time, particle->operator.size_change.end_time,
                particle->operator.size_change.start_value, particle->operator.size_change.end_value);
        instance->size = instance->initial_size * multiplier;
    }

    if(particle->operator.color_change.enabled && instance->lifetime > 0.0f) {
        float life = instance->age / instance->lifetime;
        for(int i = 0; i < 3; i++) {
            float multiplier =
                fade_value(life, particle->operator.color_change.start_time, particle->operator.color_change.end_time,
                    particle->operator.color_change.start_value[i], particle->operator.color_change.end_value[i]);
            instance->color[i] = instance->initial_color[i] * multiplier;
        }
    }

    if(particle->operator.alpha_fade.enabled && instance->lifetime > 0.0f) {
        float life = instance->age / instance->lifetime;
        float fade = 1.0f;
        if(life <= particle->operator.alpha_fade.fade_in_time) {
            fade = fade_value(life, 0.0f, particle->operator.alpha_fade.fade_in_time, 0.0f, 1.0f);
        } else if(life > particle->operator.alpha_fade.fade_out_time) {
            fade = 1.0f - fade_value(life, particle->operator.alpha_fade.fade_out_time, 1.0f, 0.0f, 1.0f);
        }
        if(fade < 0.0f) {
            fade = 0.0f;
        } else if(fade > 1.0f) {
            fade = 1.0f;
        }
        instance->alpha = instance->initial_alpha * fade;
    }

    instance->frame = particle_spritesheet_frame(particle, instance);
}

static void update_particle(wpe_particle_object* particle, float delta) {
    for(int i = 0; i < particle->num_emitters; i++) {
        if(particle->emitters[i].interval <= 0.0f) {
            continue;
        }
        particle->emitters[i].timer += delta;
        while(particle->emitters[i].timer >= particle->emitters[i].interval) {
            particle->emitters[i].timer -= particle->emitters[i].interval;
            spawn_particle_instance(particle, &particle->emitters[i], particle->emitters[i].interval);
        }
    }
    for(int i = 0; i < particle->max_count; i++) {
        update_particle_instance(particle, &particle->instances[i], delta);
    }
}

static void update_particle_instance_data(wpe_particle_object* particle) {
    for(int i = 0; i < particle->max_count; i++) {
        if(!particle->instances[i].alive) {
            particle->instance_data[i] = (wpe_particle_instance_data){0};
            continue;
        }
        particle->instance_data[i] = (wpe_particle_instance_data){
            .position =
                {
                    particle->instances[i].position[0],
                    particle->instances[i].position[1],
                    particle->instances[i].position[2],
                },
            .rotation =
                {
                    particle->instances[i].rotation[0],
                    particle->instances[i].rotation[1],
                    particle->instances[i].rotation[2],
                },
            .size = particle->instances[i].size,
            .color =
                {
                    particle->instances[i].color[0],
                    particle->instances[i].color[1],
                    particle->instances[i].color[2],
                    particle->instances[i].alpha,
                },
            .frame = particle->instances[i].frame,
        };
    }
}

static void ensure_particle_vertex_buffer() {
    if(particle_vertex_buffer.id != 0) {
        return;
    }
    particle_vertex_buffer = ow_create_vertex_buffer(sizeof(particle_vertex_data));
    ow_update_vertex_buffer(particle_vertex_buffer, 0, particle_vertex_data, sizeof(particle_vertex_data));
}

static void resolve_particle_material(wpe_material* material) {
    material->shader = wpe_find_shader(material->shader_id);
    if(material->shader != NULL) {
        wpe_init_shader(material->shader);
    }
    for(int i = 0; i < material->num_textures; i++) {
        material->textures[i].texture = wpe_find_texture(material->textures[i].texture_id);
        if(material->textures[i].texture == NULL) {
            material->textures[i].texture = wpe_find_texture_by_name(material->textures[i].name);
        }
        wpe_init_texture(material->textures[i].texture);
    }
}

static ow_pipeline_id create_particle_pipeline(wpe_shader* shader, const char* blending) {
    if(shader == NULL || shader->vertex_shader.id == 0 || shader->fragment_shader.id == 0) {
        return (ow_pipeline_id){0};
    }

    ow_vertex_binding_info vertex_bindings[2] = {
        {.slot = 0, .stride = sizeof(wpe_particle_vertex_data)},
        {.slot = 1, .stride = sizeof(wpe_particle_instance_data), .per_instance = true},
    };

    return ow_create_pipeline(&(ow_pipeline_info){
        .vertex_bindings = vertex_bindings,
        .vertex_bindings_count = 2,
        .vertex_attributes = particle_vertex_attributes,
        .vertex_attributes_count = 6,
        .vertex_shader = shader->vertex_shader,
        .fragment_shader = shader->fragment_shader,
        .color_target_format = OW_TEXTURE_RGBA8_UNORM,
        .blend_mode = wpe_blend_mode_from_name(blending),
        .depth_test_mode = OW_DEPTHTEST_DISABLED,
        .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
        .cull_mode = OW_CULL_NONE,
    });
}

void wpe_renderer_init_particle_object(wpe_object* obj) {
    if(obj->type != OBJECTTYPE_PARTICLE || obj->particle.max_count <= 0) {
        return;
    }

    ensure_particle_vertex_buffer();
    wpe_particle_object* particle = &obj->particle;
    resolve_particle_material(&particle->material);
    if(particle->material.shader == NULL || wpe_material_texture_at(&particle->material, 0) == NULL) {
        return;
    }

    for(int i = 0; i < particle->num_emitters; i++) {
        if(particle->emitters[i].rate > 0.0f) {
            particle->emitters[i].interval = 1.0f / particle->emitters[i].rate;
        }
    }

    particle->instances = calloc((size_t)particle->max_count, sizeof(wpe_particle_instance));
    particle->instance_data = calloc((size_t)particle->max_count, sizeof(wpe_particle_instance_data));
    particle->instance_buffer =
        ow_create_vertex_buffer(sizeof(wpe_particle_instance_data) * (size_t)particle->max_count);
    particle->pipeline = create_particle_pipeline(particle->material.shader, particle->material.blending);

    for(float timer = 0.0f; timer < particle->start_time; timer += 0.05f) {
        update_particle(particle, 0.05f);
    }
    update_particle_instance_data(particle);
    ow_update_vertex_buffer(particle->instance_buffer, 0, particle->instance_data,
        sizeof(wpe_particle_instance_data) * particle->max_count);
}

static bool particle_ready_for_update(wpe_particle_object* particle) {
    return particle->max_count > 0 && particle->instances != NULL && particle->instance_data != NULL &&
           particle->instance_buffer.id != 0;
}

void wpe_renderer_update_particle_objects(float delta) {
    bool needs_upload = false;

    for(size_t i = 0; i < scene.num_objects; i++) {
        wpe_object* object = &scene.objects[i];
        if(object->type != OBJECTTYPE_PARTICLE) {
            continue;
        }

        wpe_particle_object* particle = &object->particle;
        if(!particle_ready_for_update(particle)) {
            continue;
        }

        update_particle(particle, delta);
        update_particle_instance_data(particle);
        needs_upload = true;
    }

    if(!needs_upload) {
        return;
    }

    ow_begin_copy_pass();
    for(size_t i = 0; i < scene.num_objects; i++) {
        wpe_object* object = &scene.objects[i];
        if(object->type != OBJECTTYPE_PARTICLE) {
            continue;
        }

        wpe_particle_object* particle = &object->particle;
        if(!particle_ready_for_update(particle)) {
            continue;
        }

        ow_update_vertex_buffer(particle->instance_buffer, 0, particle->instance_data,
            sizeof(wpe_particle_instance_data) * particle->max_count);
    }
    ow_end_copy_pass();
}

bool wpe_renderer_render_particle_object(wpe_object* object, bool clear, const wpe_renderer_state* state) {
    wpe_particle_object* particle = &object->particle;
    wpe_texture* texture = wpe_material_texture_at(&particle->material, 0);
    if(particle->pipeline.id == 0 || texture == NULL || texture->texture.id == 0 || particle->instance_buffer.id == 0 ||
        !wpe_screen_snapshot_available()) {
        return false;
    }

    wpe_texture_target snapshot_target = wpe_next_screen_snapshot();
    if(!wpe_prepare_next_screen_snapshot(object, clear, state)) {
        return false;
    }

    wpe_transform_matrices matrices =
        wpe_compute_transform_matrices(wpe_transform_parameters_for_object(object, state));
    wpe_particle_vertex_uniforms vertex_uniforms = {
        .mvp = matrices.model_view_projection,
        .orientation_up = matrices.orientation_up,
        .orientation_right = matrices.orientation_right,
        .orientation_forward = matrices.orientation_forward,
        .texture_ratio = particle->texture_ratio,
    };
    wpe_particle_fragment_uniforms fragment_uniforms = {
        .spritesheet_size = {particle->spritesheet_cols, particle->spritesheet_rows},
        .screen_size = {(float)state->screen_width, (float)state->screen_height},
    };

    ow_vertex_buffer_id vertex_buffers[2] = {
        particle_vertex_buffer,
        particle->instance_buffer,
    };
    ow_texture_binding texture_binding = {
        .slot = 0,
        .texture = texture->texture,
        .sampler = wpe_sampler_for_texture(texture),
    };

    ow_begin_render_pass(&(ow_render_pass_info){
        .color_target = snapshot_target.texture,
        .clear_color = clear,
        .clear_color_rgba = {0.0f, 0.0f, 0.0f, 1.0f},
    });
    ow_push_vertex_uniform_data(0, &vertex_uniforms, sizeof(vertex_uniforms));
    ow_push_fragment_uniform_data(0, &fragment_uniforms, sizeof(fragment_uniforms));
    ow_render_geometry(particle->pipeline,
        &(ow_bindings_info){
            .vertex_buffers = vertex_buffers,
            .vertex_buffers_count = 2,
            .texture_bindings = &texture_binding,
            .texture_bindings_count = 1,
        },
        0, 4, (uint32_t)particle->max_count);
    ow_end_render_pass();
    wpe_commit_screen_snapshot();
    return true;
}
