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

static float lerp_random(float min, float max, float random) {
    return min + (max - min) * random;
}

static float oscillate_scalar_factor(wpe_particle_oscillate_scalar_operator* op, float age, float random) {
    float frequency = lerp_random(op->frequency_min, op->frequency_max, random);
    float phase = lerp_random(op->phase_min, op->phase_max, random);
    float wave = sinf((age + phase) * frequency) * 0.5f + 0.5f;
    return op->scale_min + random * (op->scale_max - op->scale_min) * wave;
}

static void apply_oscillate_position(
    wpe_particle_oscillate_position_operator* op, wpe_particle_instance* instance, float delta) {
    const float tau = (float)(2.0 * M_PI);
    float random = instance->oscillate_random;
    float frequency = lerp_random(op->frequency_min, op->frequency_max, random);
    float phase = lerp_random(op->phase_min, op->phase_max, random);
    float scale = lerp_random(op->scale_min, op->scale_max, random);
    float previous_age = instance->age - delta;

    float xz_current = sinf((instance->age + phase) * frequency);
    float xz_previous = sinf((previous_age + phase) * frequency);
    float xz_delta = (xz_current - xz_previous) * scale;
    instance->position[0] += xz_delta * op->mask[0];
    instance->position[2] += xz_delta * op->mask[2];

    float y_phase = phase + random * tau;
    float y_current = sinf((instance->age + y_phase) * frequency);
    float y_previous = sinf((previous_age + y_phase) * frequency);
    instance->position[1] += (y_current - y_previous) * op->mask[1] * scale;
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

static float vec3_length(float v[3]) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void vec3_normalize(float v[3]) {
    float len = vec3_length(v);
    if(len > 0.0001f) {
        float inv = 1.0f / len;
        v[0] *= inv;
        v[1] *= inv;
        v[2] *= inv;
    }
}

static float vec3_dot(float a[3], float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void vec3_cross(float a[3], float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static void vec3_copy(float src[3], float dst[3]) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void axis_angle_rotate(float v[3], float axis_in[3], float angle, float out[3]) {
    float axis[3];
    vec3_copy(axis_in, axis);
    vec3_normalize(axis);

    float c = cosf(angle);
    float s = sinf(angle);
    float dot = vec3_dot(axis, v);
    float cross[3];
    vec3_cross(axis, v, cross);
    out[0] = v[0] * c + cross[0] * s + axis[0] * dot * (1.0f - c);
    out[1] = v[1] * c + cross[1] * s + axis[1] * dot * (1.0f - c);
    out[2] = v[2] * c + cross[2] * s + axis[2] * dot * (1.0f - c);
}

static const uint8_t perlin_perm[512] = {151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36,
    103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203,
    117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48,
    27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143,
    54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207,
    206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153,
    101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
    228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214,
    31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29,
    24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180, 151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194,
    233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62,
    94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74,
    165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46,
    245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135,
    130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126,
    255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44,
    154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185,
    112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14,
    239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236,
    205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180};

static float perlin_gradient(uint8_t hash, float x) {
    float gradient = (float)((hash & 7) + 1);
    if((hash & 8) != 0) {
        gradient = -gradient;
    }
    return gradient * x;
}

static float perlin_noise(float x) {
    int cell = (int)x;
    if((float)cell > x) {
        cell--;
    }

    float t = x - (float)cell;
    float t1 = t - 1.0f;
    uint8_t hash0 = perlin_perm[(uint8_t)cell];
    uint8_t hash1 = perlin_perm[(uint8_t)(cell + 1)];

    float a0 = 1.0f - t * t;
    float a1 = 1.0f - t1 * t1;
    a0 *= a0;
    a1 *= a1;

    return (perlin_gradient(hash1, t1) * a1 * a1 + perlin_gradient(hash0, t) * a0 * a0) * 0.395f;
}

static float clamp_audio_response(float value) {
    if(value < 1.0f) {
        if(value < 0.0f) {
            return 0.0f;
        }
        if(value < 1.0f) {
            return value;
        }
    }
    return 1.0f;
}

static float turbulent_audio_response_value(wpe_turbulent_audio_response* audio, const wpe_renderer_state* state) {
    if(audio->mode == 0 || state == NULL) {
        return 0.0f;
    }

    int start = audio->frequency_start;
    int end = audio->frequency_end;
    if(start < 0) {
        start = 0;
    } else if(start > 15) {
        start = 15;
    }
    if(end < 0) {
        end = 0;
    } else if(end > 15) {
        end = 15;
    }
    if(end < start) {
        int temp = start;
        start = end;
        end = temp;
    }

    float value = 0.0f;
    if(audio->mode >= 1 && audio->mode <= 3) {
        for(int i = start; i <= end; i++) {
            float spectrum = wpe_audio_spectrum_value(state, 16, i);
            if(spectrum > value) {
                value = spectrum;
            }
        }
    }

    float min = audio->bounds[0];
    float max = audio->bounds[1];
    value = clamp_audio_response((value - min) / (max - min));
    value = (3.0f - value * 2.0f) * value * value;
    return clamp_audio_response(powf(value, audio->exponent));
}

static void generate_turbulent_velocity(
    wpe_particle_initializer* init, const wpe_renderer_state* state, float out_velocity[3]) {
    float phase_range = init->turbulent_phase_max - init->turbulent_phase_min;
    if(init->turbulent_audio.mode != 0) {
        phase_range *= turbulent_audio_response_value(&init->turbulent_audio, state);
    }

    float phase = rand_float(0.0f, 1.0f) * phase_range + init->turbulent_phase_min;
    if(state != NULL) {
        phase += state->time_seconds;
    }
    phase *= init->turbulent_time_scale;

    float angle = perlin_noise(phase) * (float)M_PI * init->turbulent_scale + init->turbulent_offset;
    float direction[3];
    axis_angle_rotate(init->turbulent_forward, init->turbulent_right, angle, direction);
    float speed = rand_float(init->turbulent_speed_min, init->turbulent_speed_max);

    out_velocity[0] = direction[0] * speed;
    out_velocity[1] = direction[1] * speed;
    out_velocity[2] = direction[2] * speed;
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

static void spawn_particle_instance(
    wpe_particle_object* particle, wpe_particle_emitter* emitter, float duration, const wpe_renderer_state* state) {
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
    if(particle->init.turbulent_velocity) {
        float turbulent_velocity[3];
        generate_turbulent_velocity(&particle->init, state, turbulent_velocity);
        for(int i = 0; i < 3; i++) {
            instance->velocity[i] += turbulent_velocity[i];
        }
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
    instance->oscillate_random = rand_float(0.0f, 1.0f);
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

    instance->size = instance->initial_size;
    instance->alpha = instance->initial_alpha;

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

    if(particle->operator.oscillate_position.enabled) {
        apply_oscillate_position(&particle->operator.oscillate_position, instance, delta);
    }

    if(particle->operator.oscillate_alpha.enabled) {
        instance->alpha *=
            oscillate_scalar_factor(&particle->operator.oscillate_alpha, instance->age, instance->oscillate_random);
    }

    if(particle->operator.oscillate_size.enabled) {
        instance->size *=
            oscillate_scalar_factor(&particle->operator.oscillate_size, instance->age, instance->oscillate_random);
    }

    instance->frame = particle_spritesheet_frame(particle, instance);
}

static void update_particle(wpe_particle_object* particle, float delta, const wpe_renderer_state* state) {
    for(int i = 0; i < particle->num_emitters; i++) {
        if(particle->emitters[i].interval <= 0.0f) {
            continue;
        }
        particle->emitters[i].timer += delta;
        while(particle->emitters[i].timer >= particle->emitters[i].interval) {
            particle->emitters[i].timer -= particle->emitters[i].interval;
            spawn_particle_instance(particle, &particle->emitters[i], particle->emitters[i].interval, state);
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
        update_particle(particle, 0.05f, NULL);
    }
    update_particle_instance_data(particle);
    ow_update_vertex_buffer(particle->instance_buffer, 0, particle->instance_data,
        sizeof(wpe_particle_instance_data) * particle->max_count);
}

static bool particle_ready_for_update(wpe_particle_object* particle) {
    return particle->max_count > 0 && particle->instances != NULL && particle->instance_data != NULL &&
           particle->instance_buffer.id != 0;
}

void wpe_renderer_update_particle_objects(float delta, const wpe_renderer_state* state) {
    bool needs_upload = false;

    for(size_t i = 0; i < scene.num_objects; i++) {
        wpe_object* object = &scene.objects[i];
        if(!object->visible || object->type != OBJECTTYPE_PARTICLE) {
            continue;
        }

        wpe_particle_object* particle = &object->particle;
        if(!particle_ready_for_update(particle)) {
            continue;
        }

        update_particle(particle, delta, state);
        update_particle_instance_data(particle);
        needs_upload = true;
    }

    if(!needs_upload) {
        return;
    }

    ow_begin_copy_pass();
    for(size_t i = 0; i < scene.num_objects; i++) {
        wpe_object* object = &scene.objects[i];
        if(!object->visible || object->type != OBJECTTYPE_PARTICLE) {
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
        .clear_color = false,
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
