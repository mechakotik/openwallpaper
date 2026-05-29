#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

#define WPE_PUPPET_NO_PARENT UINT32_MAX

#define WPE_MDL_FLAG_NORMAL 0x00000002u
#define WPE_MDL_FLAG_TANGENT 0x00000004u
#define WPE_MDL_FLAG_UV 0x00000008u
#define WPE_MDL_FLAG_UV2 0x00000020u
#define WPE_MDL_FLAG_EXTRA4 0x00010000u
#define WPE_MDL_FLAG_SKIN_BLEND 0x00800000u
#define WPE_MDL_FLAG_SKIN_WEIGHT 0x01000000u

typedef struct {
    uint8_t* data;
    size_t size;
    size_t offset;
    bool failed;
} wpe_mdl_reader;

static wpe_mat4 mat4_multiply(wpe_mat4 a, wpe_mat4 b) {
    wpe_mat4 res;
    for(int col = 0; col < 4; col++) {
        for(int row = 0; row < 4; row++) {
            res.at[col][row] = 0.0f;
            for(int k = 0; k < 4; k++) {
                res.at[col][row] += a.at[k][row] * b.at[col][k];
            }
        }
    }
    return res;
}

static bool mdl_seek(wpe_mdl_reader* reader, size_t offset) {
    if(offset > reader->size) {
        reader->failed = true;
        return false;
    }
    reader->offset = offset;
    return true;
}

static bool mdl_skip(wpe_mdl_reader* reader, size_t size) {
    if(size > reader->size - reader->offset) {
        reader->failed = true;
        return false;
    }
    reader->offset += size;
    return true;
}

static bool mdl_read_bytes(wpe_mdl_reader* reader, void* out, size_t size) {
    if(size > reader->size - reader->offset) {
        reader->failed = true;
        return false;
    }
    if(out != NULL && size > 0) {
        memcpy(out, reader->data + reader->offset, size);
    }
    reader->offset += size;
    return true;
}

static uint8_t mdl_read_u8(wpe_mdl_reader* reader) {
    uint8_t value = 0;
    (void)mdl_read_bytes(reader, &value, sizeof(value));
    return value;
}

static uint16_t mdl_read_u16(wpe_mdl_reader* reader) {
    uint8_t data[2] = {0};
    (void)mdl_read_bytes(reader, data, sizeof(data));
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t mdl_read_u32(wpe_mdl_reader* reader) {
    uint8_t data[4] = {0};
    (void)mdl_read_bytes(reader, data, sizeof(data));
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int32_t mdl_read_i32(wpe_mdl_reader* reader) {
    return (int32_t)mdl_read_u32(reader);
}

static float mdl_read_float(wpe_mdl_reader* reader) {
    uint32_t bits = mdl_read_u32(reader);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool mdl_read_float_array(wpe_mdl_reader* reader, float* values, int count) {
    for(int i = 0; i < count; i++) {
        values[i] = mdl_read_float(reader);
    }
    return !reader->failed;
}

static bool mdl_read_u32_array(wpe_mdl_reader* reader, uint32_t* values, int count) {
    for(int i = 0; i < count; i++) {
        values[i] = mdl_read_u32(reader);
    }
    return !reader->failed;
}

static const char* mdl_read_cstring(wpe_mdl_reader* reader) {
    if(reader->offset >= reader->size) {
        reader->failed = true;
        return "";
    }

    const char* value = (const char*)(reader->data + reader->offset);
    while(reader->offset < reader->size && reader->data[reader->offset] != 0) {
        reader->offset++;
    }
    if(reader->offset >= reader->size) {
        reader->failed = true;
        return "";
    }
    reader->offset++;
    return value;
}

static char* mdl_strdup(const char* value) {
    size_t len = strlen(value);
    char* copy = malloc(len + 1);
    if(copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, len + 1);
    return copy;
}

static bool mdl_peek_magic(wpe_mdl_reader* reader, const char* magic) {
    return reader->offset + 4 <= reader->size && memcmp(reader->data + reader->offset, magic, 4) == 0;
}

static int mdl_tag_version(const char tag[9]) {
    for(int i = 4; i < 8; i++) {
        if(tag[i] < '0' || tag[i] > '9') {
            return -1;
        }
    }
    return (tag[4] - '0') * 1000 + (tag[5] - '0') * 100 + (tag[6] - '0') * 10 + (tag[7] - '0');
}

static int mdl_read_version(wpe_mdl_reader* reader) {
    char tag[9] = {0};
    if(!mdl_read_bytes(reader, tag, sizeof(tag))) {
        return -1;
    }
    if(tag[0] != 'M' || tag[1] != 'D' || tag[2] != 'L') {
        reader->failed = true;
        return -1;
    }
    int version = mdl_tag_version(tag);
    if(version < 0) {
        reader->failed = true;
    }
    return version;
}

static uint32_t puppet_vertex_stride(uint32_t flag) {
    uint32_t stride = 12;
    if((flag & WPE_MDL_FLAG_NORMAL) != 0) {
        stride += 12;
    }
    if((flag & WPE_MDL_FLAG_TANGENT) != 0) {
        stride += 16;
    }
    if((flag & WPE_MDL_FLAG_EXTRA4) != 0) {
        stride += 4;
    }
    if((flag & WPE_MDL_FLAG_SKIN_BLEND) != 0) {
        stride += 16;
    }
    if((flag & WPE_MDL_FLAG_SKIN_WEIGHT) != 0) {
        stride += 16;
    }
    if((flag & (WPE_MDL_FLAG_UV | WPE_MDL_FLAG_UV2)) != 0) {
        stride += 8;
    }
    if((flag & WPE_MDL_FLAG_UV2) != 0) {
        stride += 8;
    }
    return stride;
}

static void free_puppet_mesh_data(wpe_puppet_mesh* mesh) {
    if(mesh == NULL) {
        return;
    }
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh->parts);
    free(mesh->material_name);
    mesh->vertices = NULL;
    mesh->indices = NULL;
    mesh->parts = NULL;
    mesh->material_name = NULL;
    mesh->num_vertices = 0;
    mesh->num_indices = 0;
    mesh->num_parts = 0;
}

static bool parse_puppet_mesh_parts(wpe_mdl_reader* reader, wpe_puppet_mesh* mesh, int mdl_version, int vertex_count) {
    uint8_t marker = mdl_read_u8(reader);
    if(reader->failed) {
        return false;
    }
    if(marker == 1) {
        uint8_t payload_marker = mdl_read_u8(reader);
        if(payload_marker != 0) {
            if(!mdl_skip(reader, 3)) {
                return false;
            }
            uint32_t payload_size = mdl_read_u32(reader);
            if(payload_size != (uint32_t)(12 * vertex_count)) {
                reader->failed = true;
                return false;
            }
            if(!mdl_skip(reader, payload_size)) {
                return false;
            }
        }
    } else if(marker != 0) {
        (void)mdl_version;
        reader->failed = true;
        return false;
    }

    uint8_t has_parts = mdl_read_u8(reader);
    if(has_parts == 0 || reader->failed) {
        return !reader->failed;
    }

    uint32_t parts_bytes = mdl_read_u32(reader);
    if(parts_bytes % 16 != 0) {
        reader->failed = true;
        return false;
    }

    mesh->num_parts = (int)(parts_bytes / 16);
    mesh->parts = calloc((size_t)mesh->num_parts, sizeof(wpe_puppet_mesh_part));
    if(mesh->parts == NULL && mesh->num_parts > 0) {
        reader->failed = true;
        return false;
    }

    for(int i = 0; i < mesh->num_parts; i++) {
        (void)mdl_read_u32(reader);
        (void)mdl_read_u32(reader);
        mesh->parts[i].start = mdl_read_u32(reader);
        mesh->parts[i].size = mdl_read_u32(reader);
    }
    return !reader->failed;
}

static bool skip_puppet_mesh_masks(wpe_mdl_reader* reader) {
    uint32_t mask_count = mdl_read_u32(reader);
    for(uint32_t i = 0; i < mask_count && !reader->failed; i++) {
        (void)mdl_skip(reader, 8);
        (void)mdl_read_cstring(reader);
        (void)mdl_skip(reader, 4);
        uint32_t first_count = mdl_read_u32(reader);
        (void)mdl_skip(reader, (size_t)first_count * 4);
        uint32_t second_count = mdl_read_u32(reader);
        (void)mdl_skip(reader, (size_t)second_count * 4);
    }
    return !reader->failed;
}

static bool parse_puppet_mesh(wpe_mdl_reader* reader, wpe_puppet_mesh* mesh, int mdl_version, uint32_t header_flag) {
    const char* material_name = mdl_read_cstring(reader);
    if(reader->failed) {
        return false;
    }
    mesh->material_name = mdl_strdup(material_name);
    if(mesh->material_name == NULL) {
        reader->failed = true;
        return false;
    }
    uint32_t flag_a = mdl_read_u32(reader);
    if(flag_a == 2) {
        (void)mdl_read_u32(reader);
    }
    if(mdl_version >= 17) {
        (void)mdl_skip(reader, 24);
    }

    uint32_t mesh_flag = header_flag;
    if(mdl_version > 14) {
        mesh_flag = mdl_read_u32(reader);
    }

    uint32_t vertex_bytes = mdl_read_u32(reader);
    uint32_t stride = puppet_vertex_stride(mesh_flag);
    if(stride == 0 || vertex_bytes % stride != 0) {
        reader->failed = true;
        return false;
    }

    mesh->num_vertices = (int)(vertex_bytes / stride);
    mesh->vertices = calloc((size_t)mesh->num_vertices, sizeof(wpe_puppet_vertex));
    if(mesh->vertices == NULL && mesh->num_vertices > 0) {
        reader->failed = true;
        return false;
    }

    for(int i = 0; i < mesh->num_vertices && !reader->failed; i++) {
        wpe_puppet_vertex* vertex = &mesh->vertices[i];
        vertex->normal[2] = 1.0f;
        vertex->tangent[0] = 1.0f;
        vertex->tangent[3] = 1.0f;
        vertex->blend_weights[0] = 1.0f;

        (void)mdl_read_float_array(reader, vertex->position, 3);
        if((mesh_flag & WPE_MDL_FLAG_NORMAL) != 0) {
            (void)mdl_read_float_array(reader, vertex->normal, 3);
        }
        if((mesh_flag & WPE_MDL_FLAG_TANGENT) != 0) {
            (void)mdl_read_float_array(reader, vertex->tangent, 4);
        }
        if((mesh_flag & WPE_MDL_FLAG_EXTRA4) != 0) {
            (void)mdl_skip(reader, 4);
        }
        if((mesh_flag & WPE_MDL_FLAG_SKIN_BLEND) != 0) {
            (void)mdl_read_u32_array(reader, vertex->blend_indices, 4);
        }
        if((mesh_flag & WPE_MDL_FLAG_SKIN_WEIGHT) != 0) {
            (void)mdl_read_float_array(reader, vertex->blend_weights, 4);
        }
        if((mesh_flag & (WPE_MDL_FLAG_UV | WPE_MDL_FLAG_UV2)) != 0) {
            (void)mdl_read_float_array(reader, vertex->texcoord, 2);
        }
        if((mesh_flag & WPE_MDL_FLAG_UV2) != 0) {
            (void)mdl_skip(reader, 8);
        }
    }

    uint32_t index_bytes = mdl_read_u32(reader);
    if(index_bytes % 6 != 0) {
        reader->failed = true;
        return false;
    }
    mesh->num_indices = (int)(index_bytes / 2);
    mesh->indices = calloc((size_t)mesh->num_indices, sizeof(uint16_t));
    if(mesh->indices == NULL && mesh->num_indices > 0) {
        reader->failed = true;
        return false;
    }
    for(int i = 0; i < mesh->num_indices; i++) {
        mesh->indices[i] = mdl_read_u16(reader);
    }

    if(mdl_version >= 21) {
        if(!parse_puppet_mesh_parts(reader, mesh, mdl_version, mesh->num_vertices)) {
            return false;
        }
        if(mdl_version > 21 && !skip_puppet_mesh_masks(reader)) {
            return false;
        }
    }
    return !reader->failed;
}

static bool skip_puppet_end_offset_block(wpe_mdl_reader* reader) {
    if(!mdl_skip(reader, 9)) {
        return false;
    }
    uint32_t end_offset = mdl_read_u32(reader);
    return mdl_seek(reader, end_offset);
}

static void free_puppet_attachments(wpe_puppet_model* puppet) {
    for(int i = 0; i < puppet->num_attachments; i++) {
        free(puppet->attachments[i].name);
    }
    free(puppet->attachments);
    puppet->attachments = NULL;
    puppet->num_attachments = 0;
}

static void prepare_puppet_bones(wpe_puppet_model* puppet) {
    if(puppet->num_bones <= 0 || puppet->bones == NULL) {
        return;
    }

    for(int i = 0; i < puppet->num_bones; i++) {
        puppet->bones[i].bind_pose = puppet->bones[i].local_bind;
        if(puppet->bones[i].parent != WPE_PUPPET_NO_PARENT && puppet->bones[i].parent < (uint32_t)i) {
            puppet->bones[i].bind_pose =
                mat4_multiply(puppet->bones[puppet->bones[i].parent].bind_pose, puppet->bones[i].local_bind);
        }
        puppet->bones[i].inv_bind = wpe_mat4_inverse_affine(puppet->bones[i].bind_pose);
    }
}

static bool parse_puppet_mdls(wpe_mdl_reader* reader, wpe_puppet_model* puppet) {
    int version = mdl_read_version(reader);
    uint32_t end_offset = mdl_read_u32(reader);
    uint16_t bone_count = mdl_read_u16(reader);
    (void)mdl_skip(reader, 2);
    if(reader->failed) {
        return false;
    }

    puppet->bones = calloc((size_t)bone_count, sizeof(wpe_puppet_bone));
    puppet->num_bones = bone_count;
    if(puppet->bones == NULL && bone_count > 0) {
        reader->failed = true;
        return false;
    }

    for(uint16_t i = 0; i < bone_count && !reader->failed; i++) {
        (void)mdl_read_cstring(reader);
        (void)mdl_read_i32(reader);
        puppet->bones[i].parent = mdl_read_u32(reader);
        if(puppet->bones[i].parent >= (uint32_t)i && puppet->bones[i].parent != WPE_PUPPET_NO_PARENT) {
            puppet->bones[i].parent = WPE_PUPPET_NO_PARENT;
        }
        uint32_t matrix_bytes = mdl_read_u32(reader);
        if(matrix_bytes != 64) {
            reader->failed = true;
            return false;
        }
        (void)mdl_read_float_array(reader, &puppet->bones[i].local_bind.at[0][0], 16);
        (void)mdl_read_cstring(reader);
    }

    if(version > 1 && reader->offset < end_offset) {
        uint16_t extras_flag = mdl_read_u16(reader);
        if(version == 2) {
            uint8_t has_world_binds = mdl_read_u8(reader);
            if(has_world_binds != 0) {
                (void)mdl_skip(reader, (size_t)bone_count * 64);
            }
            (void)mdl_skip(reader, 8);
        } else {
            (void)mdl_skip(reader, 9);
            if(extras_flag == 2) {
                (void)mdl_seek(reader, end_offset);
            }
        }
        if(reader->offset < end_offset) {
            uint8_t has_offset_trans = mdl_read_u8(reader);
            if(has_offset_trans != 0) {
                (void)mdl_skip(reader, (size_t)bone_count * (12 + 64));
            }
            uint8_t has_index = mdl_read_u8(reader);
            if(has_index != 0) {
                (void)mdl_skip(reader, (size_t)bone_count * 4);
            }
            if(version >= 3 && reader->offset < end_offset) {
                uint8_t has_depth = mdl_read_u8(reader);
                if(has_depth != 0) {
                    (void)mdl_skip(reader, (size_t)bone_count * 4);
                }
            }
        }
    }

    return mdl_seek(reader, end_offset);
}

static bool parse_puppet_mdat(wpe_mdl_reader* reader, wpe_puppet_model* puppet) {
    if(!mdl_skip(reader, 9)) {
        return false;
    }
    uint32_t end_offset = mdl_read_u32(reader);
    uint16_t attachment_count = mdl_read_u16(reader);
    if(reader->failed) {
        return false;
    }

    puppet->attachments = calloc((size_t)attachment_count, sizeof(wpe_puppet_attachment));
    puppet->num_attachments = attachment_count;
    if(puppet->attachments == NULL && attachment_count > 0) {
        reader->failed = true;
        return false;
    }

    for(uint16_t i = 0; i < attachment_count && !reader->failed; i++) {
        wpe_puppet_attachment* attachment = &puppet->attachments[i];
        attachment->bone_index = mdl_read_u16(reader);
        const char* name = mdl_read_cstring(reader);
        attachment->name = mdl_strdup(name);
        if(attachment->name == NULL) {
            reader->failed = true;
            free_puppet_attachments(puppet);
            return false;
        }
        (void)mdl_read_float_array(reader, &attachment->local_transform.at[0][0], 16);
    }

    if(reader->failed) {
        free_puppet_attachments(puppet);
        return false;
    }
    return mdl_seek(reader, end_offset);
}

static bool skip_puppet_animation_trans(wpe_mdl_reader* reader) {
    uint32_t trans_flag = mdl_read_u32(reader);
    if(trans_flag != 1) {
        return !reader->failed;
    }
    uint32_t extra_size = mdl_read_u32(reader);
    (void)mdl_skip(reader, extra_size);
    if(extra_size > 0) {
        (void)mdl_skip(reader, 4);
    }
    uint32_t main_size = mdl_read_u32(reader);
    (void)mdl_skip(reader, main_size);
    if(extra_size > 0) {
        (void)mdl_skip(reader, 4);
    }
    return !reader->failed;
}

static bool skip_puppet_animation_bone_curves(wpe_mdl_reader* reader, int bone_count) {
    uint8_t has_curves = mdl_read_u8(reader);
    if(has_curves == 0 || reader->failed) {
        return !reader->failed;
    }
    for(int i = 0; i < bone_count && !reader->failed; i++) {
        (void)mdl_skip(reader, 4);
        uint32_t byte_size = mdl_read_u32(reader);
        (void)mdl_skip(reader, byte_size);
    }
    return !reader->failed;
}

static bool skip_puppet_animation_v4_events(wpe_mdl_reader* reader) {
    uint8_t has_events = mdl_read_u8(reader);
    if(has_events == 0 || reader->failed) {
        return !reader->failed;
    }
    uint32_t event_count = mdl_read_u32(reader);
    for(uint32_t i = 0; i < event_count && !reader->failed; i++) {
        (void)mdl_skip(reader, 8);
        uint32_t byte_size = mdl_read_u32(reader);
        (void)mdl_skip(reader, byte_size);
    }
    return !reader->failed;
}

static bool parse_puppet_animation(wpe_mdl_reader* reader, wpe_puppet_animation* animation, int mdla_version) {
    animation->id = mdl_read_i32(reader);
    (void)mdl_skip(reader, 4);
    const char* name = mdl_read_cstring(reader);
    if(name[0] == '\0') {
        (void)mdl_read_cstring(reader);
    }
    const char* mode = mdl_read_cstring(reader);
    if(strcmp(mode, "mirror") == 0) {
        animation->mode = WPE_PUPPET_PLAY_MIRROR;
    } else if(strcmp(mode, "single") == 0) {
        animation->mode = WPE_PUPPET_PLAY_SINGLE;
    } else {
        animation->mode = WPE_PUPPET_PLAY_LOOP;
    }
    animation->fps = mdl_read_float(reader);
    animation->length = mdl_read_i32(reader);
    (void)mdl_skip(reader, 4);

    uint32_t track_count = mdl_read_u32(reader);
    animation->num_bone_tracks = (int)track_count;
    animation->bone_tracks = calloc((size_t)animation->num_bone_tracks, sizeof(wpe_puppet_bone_track));
    if(animation->bone_tracks == NULL && animation->num_bone_tracks > 0) {
        reader->failed = true;
        return false;
    }

    for(int track_idx = 0; track_idx < animation->num_bone_tracks && !reader->failed; track_idx++) {
        (void)mdl_skip(reader, 4);
        uint32_t byte_size = mdl_read_u32(reader);
        if(byte_size % 36 != 0) {
            reader->failed = true;
            return false;
        }
        wpe_puppet_bone_track* track = &animation->bone_tracks[track_idx];
        track->num_frames = (int)(byte_size / 36);
        track->frames = calloc((size_t)track->num_frames, sizeof(wpe_puppet_bone_frame));
        if(track->frames == NULL && track->num_frames > 0) {
            reader->failed = true;
            return false;
        }
        for(int frame_idx = 0; frame_idx < track->num_frames && !reader->failed; frame_idx++) {
            wpe_puppet_bone_frame* frame = &track->frames[frame_idx];
            (void)mdl_read_float_array(reader, frame->position.at, 3);
            (void)mdl_read_float_array(reader, frame->angle.at, 3);
            (void)mdl_read_float_array(reader, frame->scale.at, 3);
        }
    }

    if(mdla_version >= 3) {
        if(!skip_puppet_animation_trans(reader) ||
            !skip_puppet_animation_bone_curves(reader, animation->num_bone_tracks)) {
            return false;
        }
    }
    if(mdla_version >= 4 && !skip_puppet_animation_v4_events(reader)) {
        return false;
    }
    if(mdla_version >= 5) {
        (void)mdl_skip(reader, 24);
    }
    if(mdla_version == 6 && !skip_puppet_animation_bone_curves(reader, animation->num_bone_tracks)) {
        return false;
    }

    uint32_t event_count = mdl_read_u32(reader);
    for(uint32_t i = 0; i < event_count && !reader->failed; i++) {
        (void)mdl_skip(reader, 4);
        (void)mdl_read_cstring(reader);
    }
    return !reader->failed;
}

static void free_puppet_animations(wpe_puppet_model* puppet) {
    for(int i = 0; i < puppet->num_animations; i++) {
        wpe_puppet_animation* animation = &puppet->animations[i];
        for(int track_idx = 0; track_idx < animation->num_bone_tracks; track_idx++) {
            free(animation->bone_tracks[track_idx].frames);
        }
        free(animation->bone_tracks);
    }
    free(puppet->animations);
    puppet->animations = NULL;
    puppet->num_animations = 0;
}

static bool parse_puppet_mdla(wpe_mdl_reader* reader, wpe_puppet_model* puppet) {
    int version = mdl_read_version(reader);
    if(version == 0) {
        return !reader->failed;
    }
    uint32_t end_offset = mdl_read_u32(reader);
    uint32_t animation_count = mdl_read_u32(reader);
    if(reader->failed) {
        return false;
    }

    puppet->num_animations = (int)animation_count;
    puppet->animations = calloc((size_t)puppet->num_animations, sizeof(wpe_puppet_animation));
    if(puppet->animations == NULL && puppet->num_animations > 0) {
        reader->failed = true;
        return false;
    }

    bool ok = true;
    for(int i = 0; i < puppet->num_animations && ok; i++) {
        ok = parse_puppet_animation(reader, &puppet->animations[i], version);
    }
    if(!ok) {
        free_puppet_animations(puppet);
        reader->failed = false;
    }
    return mdl_seek(reader, end_offset);
}

static bool parse_puppet_model(wpe_puppet_model* puppet, uint8_t* data, size_t size) {
    wpe_mdl_reader reader = {
        .data = data,
        .size = size,
    };

    int version = mdl_read_version(&reader);
    uint32_t header_flag = mdl_read_u32(&reader);
    uint32_t marker = mdl_read_u32(&reader);
    uint32_t mesh_count = mdl_read_u32(&reader);
    if(reader.failed || marker != 1) {
        return false;
    }

    puppet->meshes = calloc((size_t)mesh_count, sizeof(wpe_puppet_mesh));
    puppet->num_meshes = 0;
    if(puppet->meshes == NULL && mesh_count > 0) {
        return false;
    }

    for(uint32_t i = 0; i < mesh_count && !reader.failed; i++) {
        wpe_puppet_mesh mesh = {0};
        if(!parse_puppet_mesh(&reader, &mesh, version, header_flag)) {
            free_puppet_mesh_data(&mesh);
            return false;
        }
        if(mesh.num_vertices > 0 && mesh.num_indices > 0) {
            puppet->meshes[puppet->num_meshes++] = mesh;
        } else {
            free_puppet_mesh_data(&mesh);
        }
    }

    while(reader.offset + 4 <= reader.size && !reader.failed) {
        if(mdl_peek_magic(&reader, "MDLS")) {
            if(!parse_puppet_mdls(&reader, puppet)) {
                return false;
            }
        } else if(mdl_peek_magic(&reader, "MDAT")) {
            if(!parse_puppet_mdat(&reader, puppet)) {
                return false;
            }
        } else if(mdl_peek_magic(&reader, "MDLA")) {
            if(!parse_puppet_mdla(&reader, puppet)) {
                free_puppet_animations(puppet);
            }
        } else if(mdl_peek_magic(&reader, "MDMP")) {
            if(!skip_puppet_end_offset_block(&reader)) {
                return false;
            }
        } else if(mdl_peek_magic(&reader, "MDLE")) {
            if(!skip_puppet_end_offset_block(&reader)) {
                return false;
            }
        } else {
            break;
        }
    }

    prepare_puppet_bones(puppet);
    return !reader.failed;
}

static bool load_puppet_model(wpe_puppet_model* puppet) {
    if(puppet == NULL || puppet->path == NULL || puppet->path[0] == '\0') {
        return false;
    }

    size_t size = ow_get_file_size(puppet->path);
    if(size == 0) {
        return false;
    }

    uint8_t* data = malloc(size);
    if(data == NULL) {
        return false;
    }

    ow_read_file(puppet->path, data);
    bool ok = parse_puppet_model(puppet, data, size);
    free(data);
    if(!ok) {
        printf("warning: skipping puppet %s because parsing failed\n", puppet->path);
    }
    return ok;
}

static void reset_puppet_pose(wpe_puppet_model* puppet) {
    for(int i = 0; i < puppet->num_bones; i++) {
        if(puppet->pose_matrices != NULL) {
            puppet->pose_matrices[i] = puppet->bones[i].bind_pose;
        }
        if(puppet->bone_matrices != NULL) {
            puppet->bone_matrices[i] = wpe_mat4_identity();
        }
    }
}

static wpe_mat4 puppet_bone_current_pose(wpe_puppet_model* puppet, uint32_t bone_idx) {
    if(puppet == NULL || bone_idx >= (uint32_t)puppet->num_bones) {
        return wpe_mat4_identity();
    }
    if(puppet->pose_matrices != NULL) {
        return puppet->pose_matrices[bone_idx];
    }
    return puppet->bones[bone_idx].bind_pose;
}

bool wpe_puppet_attachment_transform(wpe_puppet_model* puppet, const char* name, wpe_mat4* transform_out) {
    if(transform_out == NULL) {
        return false;
    }
    *transform_out = wpe_mat4_identity();
    if(puppet == NULL || name == NULL || name[0] == '\0') {
        return false;
    }

    for(int i = 0; i < puppet->num_attachments; i++) {
        wpe_puppet_attachment* attachment = &puppet->attachments[i];
        if(attachment->name == NULL || strcmp(attachment->name, name) != 0) {
            continue;
        }
        if((uint32_t)attachment->bone_index >= (uint32_t)puppet->num_bones) {
            return false;
        }

        wpe_mat4 bone_pose = puppet_bone_current_pose(puppet, attachment->bone_index);
        *transform_out = mat4_multiply(bone_pose, attachment->local_transform);
        return true;
    }
    return false;
}

void wpe_init_puppet_model(wpe_puppet_model* puppet) {
    if(puppet == NULL) {
        return;
    }
    if(puppet->path != NULL && puppet->meshes == NULL && puppet->bones == NULL && puppet->animations == NULL) {
        (void)load_puppet_model(puppet);
    }

    for(int mesh_idx = 0; mesh_idx < puppet->num_meshes; mesh_idx++) {
        wpe_puppet_mesh* mesh = &puppet->meshes[mesh_idx];
        if(mesh->num_vertices > 0 && mesh->vertices != NULL && mesh->vertex_buffer.id == 0) {
            uint32_t size = (uint32_t)(sizeof(wpe_puppet_vertex) * (size_t)mesh->num_vertices);
            mesh->vertex_buffer = ow_create_vertex_buffer(size);
            ow_update_vertex_buffer(mesh->vertex_buffer, 0, mesh->vertices, size);
        }
        if(mesh->num_indices > 0 && mesh->indices != NULL && mesh->index_buffer.id == 0) {
            uint32_t size = (uint32_t)(sizeof(uint16_t) * (size_t)mesh->num_indices);
            mesh->index_buffer = ow_create_index_buffer(size, false);
            ow_update_index_buffer(mesh->index_buffer, 0, mesh->indices, size);
        }
        free(mesh->vertices);
        free(mesh->indices);
        mesh->vertices = NULL;
        mesh->indices = NULL;
    }

    if(puppet->num_bones > 0 && (puppet->pose_matrices == NULL || puppet->bone_matrices == NULL)) {
        if(puppet->pose_matrices == NULL) {
            puppet->pose_matrices = calloc((size_t)puppet->num_bones, sizeof(wpe_mat4));
        }
        if(puppet->bone_matrices == NULL) {
            puppet->bone_matrices = calloc((size_t)puppet->num_bones, sizeof(wpe_mat4));
        }
        reset_puppet_pose(puppet);
    }
}
