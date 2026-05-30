#include "defs.h"

static ow_texture_id screen_snapshots[2];
static uint32_t screen_snapshot_width[2];
static uint32_t screen_snapshot_height[2];
static int screen_snapshot_index;

static void set_background_clear_color(ow_render_pass_info* pass_info) {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if(scene.general.clear_enabled) {
        r = scene.general.clear_color.r;
        g = scene.general.clear_color.g;
        b = scene.general.clear_color.b;
    }
    pass_info->clear_color_rgba[0] = r;
    pass_info->clear_color_rgba[1] = g;
    pass_info->clear_color_rgba[2] = b;
    pass_info->clear_color_rgba[3] = 1.0f;
}

static void clear_texture_to_background(ow_texture_id texture) {
    ow_render_pass_info pass_info = {
        .color_target = texture,
        .clear_color = true,
    };
    set_background_clear_color(&pass_info);
    ow_begin_render_pass(&pass_info);
    ow_end_render_pass();
}

wpe_texture_target wpe_current_screen_snapshot() {
    return (wpe_texture_target){
        .texture = screen_snapshots[screen_snapshot_index],
        .width = screen_snapshot_width[screen_snapshot_index],
        .height = screen_snapshot_height[screen_snapshot_index],
    };
}

wpe_texture_target wpe_next_screen_snapshot() {
    int next_index = 1 - screen_snapshot_index;
    return (wpe_texture_target){
        .texture = screen_snapshots[next_index],
        .width = screen_snapshot_width[next_index],
        .height = screen_snapshot_height[next_index],
    };
}

bool wpe_screen_snapshot_available() {
    return screen_snapshots[0].id != 0 && screen_snapshots[1].id != 0;
}

void wpe_commit_screen_snapshot() {
    screen_snapshot_index = 1 - screen_snapshot_index;
}

bool wpe_prepare_next_screen_snapshot(wpe_object* object, bool clear, const wpe_renderer_state* state) {
    if(!wpe_screen_snapshot_available()) {
        return false;
    }
    if(clear) {
        clear_texture_to_background(wpe_next_screen_snapshot().texture);
        return true;
    }

    wpe_texture_target previous = wpe_current_screen_snapshot();
    wpe_texture_target target = wpe_next_screen_snapshot();
    return wpe_render_texture_with_passthrough(object, previous, wpe_passthrough_pipeline_for_blending("normal", true),
        target.texture, true, wpe_default_transform_matrices(), state);
}

void wpe_renderer_init() {
    wpe_renderer_common_init();
    wpe_resolve_object_parents();
}

void wpe_renderer_begin_frame(const wpe_renderer_state* state) {
    if(wpe_passthrough_available()) {
        for(int i = 0; i < 2; i++) {
            wpe_ensure_texture_size(&screen_snapshots[i], &screen_snapshot_width[i], &screen_snapshot_height[i],
                state->screen_width, state->screen_height);
        }
        screen_snapshot_index = 0;
        clear_texture_to_background(screen_snapshots[screen_snapshot_index]);
    }
}

void wpe_renderer_init_object(wpe_object* obj) {
    if(obj->type == OBJECTTYPE_IMAGE) {
        wpe_renderer_init_image_object(obj);
    } else if(obj->type == OBJECTTYPE_PARTICLE) {
        wpe_renderer_init_particle_object(obj);
    }
}

void wpe_renderer_end_frame(const wpe_renderer_state* state) {
    if(!wpe_screen_snapshot_available()) {
        ow_render_pass_info pass_info = {
            .color_target = (ow_texture_id){0},
            .clear_color = true,
        };
        set_background_clear_color(&pass_info);
        ow_begin_render_pass(&pass_info);
        ow_end_render_pass();
        return;
    }

    wpe_texture_target source = wpe_current_screen_snapshot();
    wpe_render_texture_with_passthrough(NULL, source, wpe_passthrough_pipeline_for_blending("normal", false),
        (ow_texture_id){0}, true, wpe_default_transform_matrices(), state);
}
