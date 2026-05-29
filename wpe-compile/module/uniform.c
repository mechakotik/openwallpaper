#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

static int align_to(int value, int alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static int uniform_type_components(const char* type) {
    if(strcmp(type, "float") == 0 || strcmp(type, "int") == 0 || strcmp(type, "uint") == 0 ||
        strcmp(type, "bool") == 0) {
        return 1;
    }
    if(strcmp(type, "vec2") == 0 || strcmp(type, "ivec2") == 0 || strcmp(type, "uvec2") == 0 ||
        strcmp(type, "bvec2") == 0) {
        return 2;
    }
    if(strcmp(type, "vec3") == 0 || strcmp(type, "ivec3") == 0 || strcmp(type, "uvec3") == 0 ||
        strcmp(type, "bvec3") == 0) {
        return 3;
    }
    if(strcmp(type, "vec4") == 0 || strcmp(type, "ivec4") == 0 || strcmp(type, "uvec4") == 0 ||
        strcmp(type, "bvec4") == 0) {
        return 4;
    }
    if(strcmp(type, "mat3") == 0) {
        return 9;
    }
    if(strcmp(type, "mat4x3") == 0) {
        return 12;
    }
    if(strcmp(type, "mat4") == 0) {
        return 16;
    }
    return 4;
}

static void uniform_type_layout(const char* type, int* alignment_out, int* size_out) {
    int alignment = 16;
    int size = 16;

    if(strcmp(type, "float") == 0 || strcmp(type, "int") == 0 || strcmp(type, "uint") == 0 ||
        strcmp(type, "bool") == 0) {
        alignment = 4;
        size = 4;
    } else if(strcmp(type, "vec2") == 0 || strcmp(type, "ivec2") == 0 || strcmp(type, "uvec2") == 0 ||
              strcmp(type, "bvec2") == 0) {
        alignment = 8;
        size = 8;
    } else if(strcmp(type, "vec3") == 0 || strcmp(type, "ivec3") == 0 || strcmp(type, "uvec3") == 0 ||
              strcmp(type, "bvec3") == 0) {
        alignment = 16;
        size = 12;
    } else if(strcmp(type, "vec4") == 0 || strcmp(type, "ivec4") == 0 || strcmp(type, "uvec4") == 0 ||
              strcmp(type, "bvec4") == 0) {
        alignment = 16;
        size = 16;
    } else if(strcmp(type, "mat3") == 0) {
        alignment = 16;
        size = 48;
    } else if(strcmp(type, "mat4") == 0 || strcmp(type, "mat4x3") == 0) {
        alignment = 16;
        size = 64;
    }

    *alignment_out = alignment;
    *size_out = size;
}

static void write_float_value(uint8_t* data, int offset, float value) {
    memcpy(data + offset, &value, sizeof(value));
}

static void write_int_value(uint8_t* data, int offset, int value) {
    memcpy(data + offset, &value, sizeof(value));
}

static void write_float_vector(uint8_t* data, int offset, const float* values, int count) {
    memcpy(data + offset, values, sizeof(float) * (size_t)count);
}

static void write_vec2(uint8_t* data, int offset, float x, float y) {
    float values[2] = {x, y};
    write_float_vector(data, offset, values, 2);
}

static void write_vec3(uint8_t* data, int offset, float x, float y, float z) {
    float values[3] = {x, y, z};
    write_float_vector(data, offset, values, 3);
}

static void write_vec4(uint8_t* data, int offset, float x, float y, float z, float w) {
    float values[4] = {x, y, z, w};
    write_float_vector(data, offset, values, 4);
}

static void write_mat3_identity(uint8_t* data, int offset) {
    wpe_mat3 mat = {0};
    mat.at[0][0] = 1.0f;
    mat.at[1][1] = 1.0f;
    mat.at[2][2] = 1.0f;
    memcpy(data + offset, &mat, sizeof(mat));
}

static void write_mat3_from_model(uint8_t* data, int offset, wpe_mat4 model) {
    wpe_mat3 mat = {0};
    for(int col = 0; col < 3; col++) {
        for(int row = 0; row < 3; row++) {
            mat.at[col][row] = model.at[col][row];
        }
    }
    memcpy(data + offset, &mat, sizeof(mat));
}

static void write_mat4(uint8_t* data, int offset, wpe_mat4 value) {
    memcpy(data + offset, &value, sizeof(value));
}

static void write_mat4x3_default_value(uint8_t* data, int offset, const float* values, int len) {
    wpe_mat4 mat = {0};
    for(int col = 0; col < 4; col++) {
        for(int row = 0; row < 3; row++) {
            int source_idx = col * 3 + row;
            if(source_idx < len) {
                mat.at[col][row] = values[source_idx];
            }
        }
    }
    memcpy(data + offset, &mat, sizeof(mat));
}

static void write_texture_resolution(uint8_t* data, int offset, wpe_texture_target* texture_slots,
    int num_texture_slots, int slot, const wpe_renderer_state* state, bool texel) {
    float width = (float)state->screen_width;
    float height = (float)state->screen_height;

    if(slot >= 0 && slot < num_texture_slots && texture_slots[slot].texture.id != 0) {
        width = (float)texture_slots[slot].width;
        height = (float)texture_slots[slot].height;
    }
    if(width <= 0.0f) {
        width = 1.0f;
    }
    if(height <= 0.0f) {
        height = 1.0f;
    }

    if(texel) {
        write_vec4(data, offset, 1.0f / width, 1.0f / height, width, height);
    } else {
        write_vec4(data, offset, width, height, width, height);
    }
}

static int audio_spectrum_size_from_uniform_name(const char* name) {
    const char* prefix = "g_AudioSpectrum";
    size_t prefix_len = strlen(prefix);
    if(name == NULL || strncmp(name, prefix, prefix_len) != 0) {
        return 0;
    }

    const char* size_start = name + prefix_len;
    if(*size_start < '0' || *size_start > '9') {
        return 0;
    }

    char* suffix = NULL;
    long size = strtol(size_start, &suffix, 10);
    if(size <= 0 || (strcmp(suffix, "Left") != 0 && strcmp(suffix, "Right") != 0)) {
        return 0;
    }
    return (int)size;
}

static float audio_spectrum_value(const wpe_renderer_state* state, int target_size, int index) {
    if(state == NULL || state->audio_spectrum == NULL || state->audio_spectrum_size <= 0 || target_size <= 0 ||
        index < 0) {
        return 0.0f;
    }

    int source_size = state->audio_spectrum_size;
    int start = index * source_size / target_size;
    int end = (index + 1) * source_size / target_size;
    if(start >= source_size) {
        start = source_size - 1;
    }
    if(end <= start) {
        end = start + 1;
    }
    if(end > source_size) {
        end = source_size;
    }

    float sum = 0.0f;
    for(int i = start; i < end; i++) {
        sum += state->audio_spectrum[i];
    }
    return sum / (float)(end - start);
}

static bool write_audio_spectrum_uniform(
    uint8_t* data, int offset, int stride, wpe_uniform_info* uniform, const wpe_renderer_state* state) {
    int spectrum_size = audio_spectrum_size_from_uniform_name(uniform->name);
    if(spectrum_size == 0) {
        return false;
    }
    if(strcmp(uniform->type, "float") != 0 || uniform->array_size <= 0) {
        return false;
    }

    for(int i = 0; i < uniform->array_size; i++) {
        float value = i < spectrum_size ? audio_spectrum_value(state, spectrum_size, i) : 0.0f;
        write_float_value(data, offset + stride * i, value);
    }
    return true;
}

static bool write_builtin_uniform(uint8_t* data, int offset, int stride, wpe_uniform_info* uniform, wpe_object* object,
    wpe_texture_target* texture_slots, int num_texture_slots, wpe_transform_matrices matrices,
    const wpe_renderer_state* state) {
    const char* name = uniform->name;
    wpe_image_object fallback_image = {
        .color = {.r = 1.0f, .g = 1.0f, .b = 1.0f},
        .alpha = 1.0f,
        .brightness = 1.0f,
    };
    wpe_image_object* image = object != NULL && object->type == OBJECTTYPE_IMAGE ? &object->image : &fallback_image;

    if(strcmp(name, "g_ModelViewProjectionMatrix") == 0) {
        write_mat4(data, offset, matrices.model_view_projection);
        return true;
    }
    if(strcmp(name, "g_ViewProjectionMatrix") == 0) {
        write_mat4(data, offset, matrices.view_projection);
        return true;
    }
    if(strcmp(name, "g_ModelMatrix") == 0 || strcmp(name, "g_AltModelMatrix") == 0 ||
        strcmp(name, "g_EffectModelMatrix") == 0) {
        write_mat4(data, offset, matrices.model);
        return true;
    }
    if(strcmp(name, "g_NormalModelMatrix") == 0 || strcmp(name, "g_AltNormalModelMatrix") == 0) {
        write_mat3_from_model(data, offset, matrices.model);
        return true;
    }
    if(strcmp(name, "g_ModelMatrixInverse") == 0) {
        write_mat4(data, offset, wpe_mat4_identity());
        return true;
    }
    if(strcmp(name, "g_Bones") == 0 && strcmp(uniform->type, "mat4x3") == 0 && uniform->array_size > 0) {
        for(int i = 0; i < uniform->array_size; i++) {
            wpe_mat4 bone = wpe_mat4_identity();
            if(image->puppet != NULL && image->puppet->bone_matrices != NULL && i < image->puppet->num_bones) {
                bone = image->puppet->bone_matrices[i];
            }
            write_mat4(data, offset + stride * i, bone);
        }
        return true;
    }
    if(strcmp(name, "g_BonesAlpha") == 0 && strcmp(uniform->type, "float") == 0 && uniform->array_size > 0) {
        for(int i = 0; i < uniform->array_size; i++) {
            write_float_value(data, offset + stride * i, 1.0f);
        }
        return true;
    }
    if(strcmp(name, "g_EffectTextureProjectionMatrix") == 0 ||
        strcmp(name, "g_EffectTextureProjectionMatrixInverse") == 0) {
        write_mat4(data, offset, wpe_mat4_identity());
        return true;
    }
    if(strcmp(name, "g_Time") == 0) {
        write_float_value(data, offset, state->time_seconds);
        return true;
    }
    if(strcmp(name, "g_Color4") == 0) {
        write_vec4(data, offset, image->color.r, image->color.g, image->color.b, image->alpha);
        return true;
    }
    if(strcmp(name, "g_Color") == 0) {
        if(strcmp(uniform->type, "vec4") == 0) {
            write_vec4(data, offset, image->color.r, image->color.g, image->color.b, image->alpha);
        } else {
            write_vec3(data, offset, image->color.r, image->color.g, image->color.b);
        }
        return true;
    }
    if(strcmp(name, "g_Brightness") == 0) {
        write_float_value(data, offset, image->brightness == 0.0f ? 1.0f : image->brightness);
        return true;
    }
    if(strcmp(name, "g_UserAlpha") == 0 || strcmp(name, "g_Alpha") == 0 || strcmp(name, "g_TintAlpha") == 0) {
        write_float_value(data, offset, image->alpha);
        return true;
    }
    if(strcmp(name, "g_TintColor") == 0 || strcmp(name, "g_EmissiveColor") == 0 ||
        strcmp(name, "g_LightAmbientColor") == 0) {
        write_vec3(data, offset, image->color.r, image->color.g, image->color.b);
        return true;
    }
    if(strcmp(name, "g_Screen") == 0) {
        if(strcmp(uniform->type, "vec2") == 0) {
            write_vec2(data, offset, (float)state->screen_width, (float)state->screen_height);
        } else if(strcmp(uniform->type, "vec4") == 0) {
            float width = state->screen_width == 0 ? 1.0f : (float)state->screen_width;
            float height = state->screen_height == 0 ? 1.0f : (float)state->screen_height;
            write_vec4(data, offset, width, height, 1.0f / width, 1.0f / height);
        } else {
            float width = state->screen_width == 0 ? 1.0f : (float)state->screen_width;
            float height = state->screen_height == 0 ? 1.0f : (float)state->screen_height;
            write_vec3(data, offset, width, height, width / height);
        }
        return true;
    }
    if(strcmp(name, "g_TexelSize") == 0 || strcmp(name, "g_TexelSizeHalf") == 0) {
        float width = state->screen_width == 0 ? 1.0f : (float)state->screen_width;
        float height = state->screen_height == 0 ? 1.0f : (float)state->screen_height;
        float scale = strcmp(name, "g_TexelSizeHalf") == 0 ? 0.5f : 1.0f;
        write_vec2(data, offset, scale / width, scale / height);
        return true;
    }
    if(strcmp(name, "g_Texture0Rotation") == 0 || wpe_ends_with(name, "Rotation")) {
        write_vec4(data, offset, 1.0f, 0.0f, 0.0f, 1.0f);
        return true;
    }
    if(strcmp(name, "g_Texture0Translation") == 0 || wpe_ends_with(name, "Translation")) {
        write_vec2(data, offset, 0.0f, 0.0f);
        return true;
    }
    if(strcmp(name, "g_OrientationUp") == 0 || strcmp(name, "g_ViewUp") == 0) {
        if(strcmp(uniform->type, "vec4") == 0) {
            write_float_vector(data, offset, matrices.orientation_up.at, 4);
        } else {
            write_float_vector(data, offset, matrices.orientation_up.at, 3);
        }
        return true;
    }
    if(strcmp(name, "g_OrientationRight") == 0 || strcmp(name, "g_ViewRight") == 0) {
        if(strcmp(uniform->type, "vec4") == 0) {
            write_float_vector(data, offset, matrices.orientation_right.at, 4);
        } else {
            write_float_vector(data, offset, matrices.orientation_right.at, 3);
        }
        return true;
    }
    if(strcmp(name, "g_OrientationForward") == 0) {
        if(strcmp(uniform->type, "vec4") == 0) {
            write_float_vector(data, offset, matrices.orientation_forward.at, 4);
        } else {
            write_float_vector(data, offset, matrices.orientation_forward.at, 3);
        }
        return true;
    }
    if(strcmp(name, "g_ParallaxPosition") == 0) {
        write_vec2(data, offset, matrices.parallax_position_x, matrices.parallax_position_y);
        return true;
    }
    if(write_audio_spectrum_uniform(data, offset, stride, uniform, state)) {
        return true;
    }

    int texture_slot = wpe_texture_slot_from_uniform_name(name);
    if(texture_slot >= 0 && wpe_ends_with(name, "Resolution")) {
        write_texture_resolution(data, offset, texture_slots, num_texture_slots, texture_slot, state, false);
        return true;
    }
    if(texture_slot >= 0 && wpe_ends_with(name, "Texel")) {
        write_texture_resolution(data, offset, texture_slots, num_texture_slots, texture_slot, state, true);
        return true;
    }
    if(texture_slot >= 0 && wpe_ends_with(name, "MipMapInfo")) {
        float width = texture_slot < num_texture_slots && texture_slots[texture_slot].width > 0
                          ? (float)texture_slots[texture_slot].width
                          : 1.0f;
        float height = texture_slot < num_texture_slots && texture_slots[texture_slot].height > 0
                           ? (float)texture_slots[texture_slot].height
                           : 1.0f;
        write_float_value(data, offset, log2f(fmaxf(width, height)));
        return true;
    }

    return false;
}

static void write_default_uniform_value(uint8_t* data, int offset, int stride, wpe_uniform_info* uniform) {
    if(uniform->array_size > 0) {
        int components = uniform_type_components(uniform->type);
        for(int i = 0; i < uniform->array_size; i++) {
            wpe_uniform_info element = *uniform;
            element.array_size = 0;
            if(uniform->default_set && uniform->default_value != NULL && uniform->default_len >= (i + 1) * components) {
                element.default_value = uniform->default_value + i * components;
                element.default_len = components;
            } else {
                element.default_value = NULL;
                element.default_len = 0;
                element.default_set = false;
            }
            write_default_uniform_value(data, offset + stride * i, stride, &element);
        }
        return;
    }

    if(uniform->default_set && uniform->default_value != NULL && uniform->default_len > 0) {
        if(strcmp(uniform->type, "int") == 0 || strcmp(uniform->type, "uint") == 0 ||
            strcmp(uniform->type, "bool") == 0) {
            write_int_value(data, offset, (int)uniform->default_value[0]);
            return;
        }

        int components = uniform_type_components(uniform->type);
        if(strcmp(uniform->type, "mat3") == 0) {
            wpe_mat3 mat = {0};
            for(int col = 0; col < 3; col++) {
                for(int row = 0; row < 3; row++) {
                    int source_idx = col * 3 + row;
                    if(source_idx < uniform->default_len) {
                        mat.at[col][row] = uniform->default_value[source_idx];
                    }
                }
            }
            memcpy(data + offset, &mat, sizeof(mat));
            return;
        }
        if(strcmp(uniform->type, "mat4x3") == 0) {
            write_mat4x3_default_value(data, offset, uniform->default_value, uniform->default_len);
            return;
        }
        if(strcmp(uniform->type, "mat4") == 0) {
            wpe_mat4 mat = {0};
            int max_components = uniform->default_len < 16 ? uniform->default_len : 16;
            for(int i = 0; i < max_components; i++) {
                mat.at[i / 4][i % 4] = uniform->default_value[i];
            }
            memcpy(data + offset, &mat, sizeof(mat));
            return;
        }
        if(components > uniform->default_len) {
            components = uniform->default_len;
        }
        write_float_vector(data, offset, uniform->default_value, components);
        return;
    }

    if(strcmp(uniform->type, "mat3") == 0) {
        write_mat3_identity(data, offset);
    } else if(strcmp(uniform->type, "mat4") == 0) {
        write_mat4(data, offset, wpe_mat4_identity());
    }
}

static wpe_uniform_constant* find_constant(wpe_uniform_constant* constants, int num_constants, const char* name) {
    if(name == NULL || name[0] == '\0') {
        return NULL;
    }
    for(int i = 0; i < num_constants; i++) {
        if(constants[i].name != NULL && strcmp(constants[i].name, name) == 0) {
            return &constants[i];
        }
    }
    return NULL;
}

uint8_t* wpe_build_uniform_data(wpe_uniform_info* uniforms, int num_uniforms, wpe_object* object,
    wpe_texture_target* texture_slots, int num_texture_slots, wpe_uniform_constant* constants, int num_constants,
    wpe_transform_matrices matrices, const wpe_renderer_state* state, int* size_out) {
    int offset = 0;

    for(int i = 0; i < num_uniforms; i++) {
        int alignment = 0;
        int size = 0;
        uniform_type_layout(uniforms[i].type, &alignment, &size);
        if(uniforms[i].array_size > 0) {
            alignment = 16;
            size = align_to(size, 16) * uniforms[i].array_size;
        }
        offset = align_to(offset, alignment);
        offset += size;
    }

    int total_size = align_to(offset, 16);
    *size_out = total_size;
    if(total_size == 0) {
        return NULL;
    }

    uint8_t* data = calloc(1, (size_t)total_size);
    offset = 0;

    for(int i = 0; i < num_uniforms; i++) {
        int alignment = 0;
        int size = 0;
        uniform_type_layout(uniforms[i].type, &alignment, &size);
        int stride = size;
        if(uniforms[i].array_size > 0) {
            alignment = 16;
            stride = align_to(size, 16);
            size = stride * uniforms[i].array_size;
        }
        offset = align_to(offset, alignment);
        if(!write_builtin_uniform(
               data, offset, stride, &uniforms[i], object, texture_slots, num_texture_slots, matrices, state)) {
            wpe_uniform_info uniform = uniforms[i];
            wpe_uniform_constant* constant = find_constant(constants, num_constants, uniform.constant_name);
            if(constant != NULL && constant->values != NULL && constant->len > 0) {
                uniform.default_value = constant->values;
                uniform.default_len = constant->len;
                uniform.default_set = true;
            }
            write_default_uniform_value(data, offset, stride, &uniform);
        }
        offset += size;
    }

    return data;
}
