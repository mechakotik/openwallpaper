#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

static wpe_renderer_state state = {
    .scale_mode = SCALE_MODE_ASPECT_CROP,
};

static void update_camera_state(float delta) {
    ow_get_screen_size(&state.screen_width, &state.screen_height);

    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    (void)ow_get_mouse_state(&mouse_x, &mouse_y);

    if(state.screen_width != 0 && state.screen_height != 0) {
        mouse_x /= (float)state.screen_width;
        mouse_y /= (float)state.screen_height;
    }

    state.mouse_x = mouse_x;
    state.mouse_y = mouse_y;

    if(state.audio_spectrum != NULL && state.audio_spectrum_size > 0) {
        ow_get_audio_spectrum(state.audio_spectrum, (size_t)state.audio_spectrum_size);
    }

    if(!state.parallax_initialized) {
        state.parallax_mouse_x = 0.5f;
        state.parallax_mouse_y = 0.5f;
        state.parallax_initialized = true;
    }

    if(scene.general.parallax) {
        float u = fmaxf(0.0f, 1.0f - mouse_y);
        float v = fmaxf(0.0f, mouse_x);
        if(u > 1.0f) {
            u = 1.0f;
        }
        if(v > 1.0f) {
            v = 1.0f;
        }

        float influence = scene.general.parallax_mouse_influence;
        float target_x = v * influence + 0.5f * (1.0f - influence);
        float target_y = u * influence + 0.5f * (1.0f - influence);

        if(scene.general.parallax_delay > 0.0f) {
            float factor = fminf(1.0f, (1.0f - scene.general.parallax_delay / 3.0f) * 10.0f * delta);
            state.parallax_mouse_x += (target_x - state.parallax_mouse_x) * factor;
            state.parallax_mouse_y += (target_y - state.parallax_mouse_y) * factor;
        } else {
            state.parallax_mouse_x = target_x;
            state.parallax_mouse_y = target_y;
        }
    } else {
        state.parallax_mouse_x = 0.5f;
        state.parallax_mouse_y = 0.5f;
    }

    state.time_seconds += delta;
}

__attribute__((export_name("init"))) void init() {
    const char* scale_mode = ow_get_option("scale-mode");
    if(scale_mode != NULL) {
        if(strcmp(scale_mode, "stretch") == 0) {
            state.scale_mode = SCALE_MODE_STRETCH;
        } else if(strcmp(scale_mode, "aspect-fit") == 0) {
            state.scale_mode = SCALE_MODE_ASPECT_FIT;
        } else if(strcmp(scale_mode, "aspect-crop") == 0) {
            state.scale_mode = SCALE_MODE_ASPECT_CROP;
        } else {
            printf("warning: unknown scale mode, defaulting to aspect-crop\n");
        }
    }

    if(scene.audio_spectrum_size > 0) {
        state.audio_spectrum_size = scene.audio_spectrum_size;
        state.audio_spectrum = calloc((size_t)state.audio_spectrum_size, sizeof(float));
    }

    ow_begin_copy_pass();
    wpe_renderer_init();
    for(size_t i = 0; i < scene.num_objects; i++) {
        wpe_renderer_init_object(&scene.objects[i]);
    }
    ow_end_copy_pass();
}

__attribute__((export_name("update"))) void update(float delta) {
    update_camera_state(delta);
    wpe_renderer_begin_frame(&state);
    wpe_renderer_update_particle_objects(delta, &state);

    bool clear = true;
    for(size_t i = 0; i < scene.num_objects; i++) {
        wpe_object* object = &scene.objects[i];
        bool rendered = false;
        if(object->type == OBJECTTYPE_IMAGE) {
            rendered = wpe_renderer_render_image_object(object, clear, &state);
        } else if(object->type == OBJECTTYPE_PARTICLE) {
            rendered = wpe_renderer_render_particle_object(object, clear, &state);
        }
        if(rendered) {
            clear = false;
        }
    }

    wpe_renderer_end_frame(&state);
}
