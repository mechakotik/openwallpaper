#include "video.h"

#ifdef WD_VIDEO

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "argparse.h"
#include "dynamic_api.h"
#include "error.h"
#include "ready.h"
#include "state.h"

extern MPV_DECLTYPE(mpv_error_string) * wd_mpv_error_string;
extern MPV_DECLTYPE(mpv_create) * wd_mpv_create;
extern MPV_DECLTYPE(mpv_initialize) * wd_mpv_initialize;
extern MPV_DECLTYPE(mpv_terminate_destroy) * wd_mpv_terminate_destroy;
extern MPV_DECLTYPE(mpv_set_option) * wd_mpv_set_option;
extern MPV_DECLTYPE(mpv_set_option_string) * wd_mpv_set_option_string;
extern MPV_DECLTYPE(mpv_command) * wd_mpv_command;
extern MPV_DECLTYPE(mpv_set_property_string) * wd_mpv_set_property_string;
extern MPV_DECLTYPE(mpv_wait_event) * wd_mpv_wait_event;
extern MPV_DECLTYPE(mpv_render_context_create) * wd_mpv_render_context_create;
extern MPV_DECLTYPE(mpv_render_context_render) * wd_mpv_render_context_render;
extern MPV_DECLTYPE(mpv_render_context_report_swap) * wd_mpv_render_context_report_swap;
extern MPV_DECLTYPE(mpv_render_context_free) * wd_mpv_render_context_free;
extern MPV_DECLTYPE(mpv_render_context_set_update_callback) * wd_mpv_render_context_set_update_callback;
extern MPV_DECLTYPE(mpv_render_context_update) * wd_mpv_render_context_update;

#define mpv_error_string wd_mpv_error_string
#define mpv_create wd_mpv_create
#define mpv_initialize wd_mpv_initialize
#define mpv_terminate_destroy wd_mpv_terminate_destroy
#define mpv_set_option wd_mpv_set_option
#define mpv_set_option_string wd_mpv_set_option_string
#define mpv_command wd_mpv_command
#define mpv_set_property_string wd_mpv_set_property_string
#define mpv_wait_event wd_mpv_wait_event
#define mpv_render_context_create wd_mpv_render_context_create
#define mpv_render_context_render wd_mpv_render_context_render
#define mpv_render_context_report_swap wd_mpv_render_context_report_swap
#define mpv_render_context_free wd_mpv_render_context_free
#define mpv_render_context_set_update_callback wd_mpv_render_context_set_update_callback
#define mpv_render_context_update wd_mpv_render_context_update

typedef void(APIENTRY* wd_gl_clear_color_fn)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void(APIENTRY* wd_gl_clear_fn)(GLbitfield mask);
typedef void(APIENTRY* wd_gl_viewport_fn)(GLint x, GLint y, GLsizei width, GLsizei height);

static wd_gl_clear_color_fn gl_clear_color;
static wd_gl_clear_fn gl_clear;
static wd_gl_viewport_fn gl_viewport;

static bool set_video_paused(wd_video_state* video, bool paused);

static bool load_gl_function(const char* name, void** target) {
    *target = SDL_GL_GetProcAddress(name);
    if(*target == NULL) {
        wd_set_error("failed to load GL function %s: %s", name, SDL_GetError());
        return false;
    }
    return true;
}

static bool load_gl_functions() {
    return load_gl_function("glClearColor", (void**)&gl_clear_color) &&
           load_gl_function("glClear", (void**)&gl_clear) && load_gl_function("glViewport", (void**)&gl_viewport);
}

static bool set_mpv_error(const char* context, int error) {
    wd_set_error("%s: %s", context, mpv_error_string(error));
    return false;
}

static void request_video_update(void* data) {
    wd_video_state* video = data;
    atomic_store_explicit(&video->update_pending, true, memory_order_release);

    SDL_Event event = {0};
    event.type = video->update_event_type;
    SDL_PushEvent(&event);
}

static void* get_proc_address(void* ctx, const char* name) {
    (void)ctx;
    return SDL_GL_GetProcAddress(name);
}

static bool set_mpv_option_string(mpv_handle* mpv, const char* name, const char* value) {
    int status = mpv_set_option_string(mpv, name, value);
    if(status < 0) {
        return set_mpv_error(name, status);
    }
    return true;
}

static bool set_mpv_speed(mpv_handle* mpv, float speed) {
    double value = speed;
    int status = mpv_set_option(mpv, "speed", MPV_FORMAT_DOUBLE, &value);
    if(status < 0) {
        return set_mpv_error("speed", status);
    }
    return true;
}

static bool drain_events(wd_video_state* video) {
    while(true) {
        mpv_event* event = mpv_wait_event(video->mpv, 0);
        if(event == NULL || event->event_id == MPV_EVENT_NONE) {
            return true;
        }

        if(event->event_id == MPV_EVENT_END_FILE) {
            mpv_event_end_file* end = event->data;
            if(end != NULL && end->reason == MPV_END_FILE_REASON_ERROR) {
                return set_mpv_error("mpv playback failed", end->error);
            }
        } else if(event->event_id == MPV_EVENT_SHUTDOWN) {
            wd_set_error("mpv backend shut down");
            return false;
        }
    }
}

static bool set_pause_state(wd_video_state* video, bool paused) {
    const char* value = paused ? "yes" : "no";
    int status = mpv_set_property_string(video->mpv, "pause", value);
    if(status < 0) {
        return set_mpv_error("pause", status);
    }
    video->paused = paused;
    return true;
}

static bool init_context(struct wd_state* state, wd_video_state* video) {
    video->context = SDL_GL_CreateContext(state->output.window);
    if(video->context == NULL) {
        wd_set_error("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    if(!SDL_GL_MakeCurrent(state->output.window, video->context)) {
        wd_set_error("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        return false;
    }
    if(!load_gl_functions()) {
        return false;
    }

    int swap_interval = state->args.fps == 0 ? 1 : 0;
    if(!SDL_GL_SetSwapInterval(swap_interval)) {
        printf("warning: failed to set OpenGL swap interval: %s\n", SDL_GetError());
    }

    return true;
}

static bool init_update_event(wd_video_state* video) {
    if(video->update_event_type != 0) {
        return true;
    }

    video->update_event_type = SDL_RegisterEvents(1);
    if(video->update_event_type == 0) {
        wd_set_error("SDL_RegisterEvents failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool init_mpv(struct wd_state* state, wd_video_state* video) {
    if(!wd_dynapi_load_mpv()) {
        wd_set_error("failed to load libmpv");
        return false;
    }

    video->mpv = mpv_create();
    if(video->mpv == NULL) {
        wd_set_error("mpv_create failed");
        return false;
    }

    if(!set_mpv_option_string(video->mpv, "vo", "libmpv") || !set_mpv_option_string(video->mpv, "audio", "no") ||
        !set_mpv_option_string(video->mpv, "loop-file", "inf") ||
        !set_mpv_option_string(video->mpv, "keep-open", "yes") ||
        !set_mpv_option_string(video->mpv, "panscan", "1.0") ||
        !set_mpv_option_string(video->mpv, "hwdec", "auto-safe")) {
        return false;
    }

    float speed = state->args.speed != 0 ? state->args.speed : 1.0f;
    if(!set_mpv_speed(video->mpv, speed)) {
        return false;
    }

    int status = mpv_initialize(video->mpv);
    if(status < 0) {
        return set_mpv_error("mpv_initialize", status);
    }

    mpv_opengl_init_params gl_init = {
        .get_proc_address = get_proc_address,
        .get_proc_address_ctx = NULL,
    };

    SDL_PropertiesID window_properties = SDL_GetWindowProperties(state->output.window);
    void* wl_display = SDL_GetPointerProperty(window_properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
    void* x11_display = SDL_GetPointerProperty(window_properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);

    mpv_render_param params[5] = {
        {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
        {0},
        {0},
        {0},
    };

    int param_index = 2;
    if(wl_display != NULL) {
        params[param_index++] = (mpv_render_param){MPV_RENDER_PARAM_WL_DISPLAY, wl_display};
    } else if(x11_display != NULL) {
        params[param_index++] = (mpv_render_param){MPV_RENDER_PARAM_X11_DISPLAY, x11_display};
    }

    status = mpv_render_context_create(&video->render_context, video->mpv, params);
    if(status < 0) {
        return set_mpv_error("mpv_render_context_create", status);
    }
    mpv_render_context_set_update_callback(video->render_context, request_video_update, video);

    const char* command[] = {"loadfile", wd_get_wallpaper_path(&state->args), NULL};
    status = mpv_command(video->mpv, command);
    if(status < 0) {
        return set_mpv_error("loadfile", status);
    }

    return drain_events(video);
}

static bool init_video(struct wd_state* state) {
    wd_video_state* video = &state->video;

    if(!init_update_event(video)) {
        return false;
    }
    if(!init_context(state, video)) {
        return false;
    }
    if(!init_mpv(state, video)) {
        return false;
    }

    return true;
}

static bool render_video(wd_state* state) {
    wd_video_state* video = &state->video;

    if(!SDL_GL_MakeCurrent(state->output.window, video->context)) {
        wd_set_error("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        return false;
    }

    int width = 0;
    int height = 0;
    if(!SDL_GetWindowSizeInPixels(state->output.window, &width, &height)) {
        wd_set_error("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
        return false;
    }
    if(width <= 0 || height <= 0) {
        return true;
    }

    gl_viewport(0, 0, width, height);
    gl_clear_color(0.f, 0.f, 0.f, 1.f);
    gl_clear(GL_COLOR_BUFFER_BIT);

    mpv_opengl_fbo fbo = {
        .fbo = 0,
        .w = width,
        .h = height,
        .internal_format = 0,
    };
    int flip_y = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {0},
    };

    int status = mpv_render_context_render(video->render_context, params);
    if(status < 0) {
        return set_mpv_error("mpv_render_context_render", status);
    }

    if(!SDL_GL_SwapWindow(state->output.window)) {
        wd_set_error("SDL_GL_SwapWindow failed: %s", SDL_GetError());
        return false;
    }
    mpv_render_context_report_swap(video->render_context);
    return true;
}

static bool update_video(wd_state* state, bool force, bool* rendered) {
    wd_video_state* video = &state->video;
    *rendered = false;

    uint64_t flags = mpv_render_context_update(video->render_context);
    if(!force && (flags & MPV_RENDER_UPDATE_FRAME) == 0) {
        return true;
    }

    if(!render_video(state)) {
        return false;
    }
    *rendered = true;
    return true;
}

static void handle_event(wd_video_state* video, SDL_Event* event, bool* quit, bool* force) {
    if(event->type == SDL_EVENT_QUIT || event->type == SDL_EVENT_TERMINATING) {
        *quit = true;
    } else if(event->type == video->update_event_type) {
        atomic_store_explicit(&video->update_pending, true, memory_order_release);
    } else if(event->type == SDL_EVENT_WINDOW_EXPOSED || event->type == SDL_EVENT_WINDOW_RESIZED ||
              event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        *force = true;
    }
}

bool wd_run_video(wd_state* state) {
    bool loaded = false;
    uint64_t last_pause_check = SDL_GetTicksNS();
    bool first_draw = true;
    bool paused = false;

    if(state->args.pause_on_bat) {
        wd_init_battery(&state->battery);
    }

    while(true) {
        SDL_Event event;
        bool quit = false;
        bool force = false;
        if(SDL_WaitEventTimeout(&event, loaded ? 200 : 0)) {
            handle_event(&state->video, &event, &quit, &force);
            while(SDL_PollEvent(&event)) {
                handle_event(&state->video, &event, &quit, &force);
            }
        }
        if(quit) {
            break;
        }

        uint64_t cur_time = SDL_GetTicksNS();

        if(!wd_update_output(&state->output)) {
            return false;
        }
        if(state->output.window == NULL) {
            if(loaded) {
                wd_free_video(&state->video);
                loaded = false;
                paused = false;
                wd_unset_ready();
                first_draw = true;
                last_pause_check = cur_time;
            }
            wd_deactivate_output(&state->output);
            SDL_Delay(200);
            continue;
        }
        if(!loaded) {
            if(!init_video(state)) {
                return false;
            }
            loaded = true;
            first_draw = true;
            last_pause_check = SDL_GetTicksNS();
            continue;
        }

        if(!drain_events(&state->video)) {
            return false;
        }

        if(!first_draw && last_pause_check < cur_time - 200000000) {
            if((state->args.pause_hidden && wd_output_hidden(&state->output)) ||
                (state->args.pause_on_bat && wd_battery_discharging(&state->battery))) {
                if(!set_video_paused(&state->video, true)) {
                    return false;
                }
                paused = true;
                SDL_Delay(200);
                continue;
            }
            last_pause_check = cur_time;
        }

        if(paused) {
            if(!set_video_paused(&state->video, false)) {
                return false;
            }
            paused = false;
            force = true;
        }

        bool update_pending = atomic_exchange_explicit(&state->video.update_pending, false, memory_order_acq_rel);
        if(update_pending || force) {
            bool rendered = false;
            if(!update_video(state, force, &rendered)) {
                return false;
            }
            if(rendered && first_draw) {
                wd_set_ready();
                first_draw = false;
            }
        }
    }

    return true;
}

static bool set_video_paused(wd_video_state* video, bool paused) {
    if(video->mpv == NULL) {
        return true;
    }
    if(video->paused == paused) {
        return true;
    }
    return set_pause_state(video, paused);
}

void wd_free_video(wd_video_state* video) {
    if(video->render_context == NULL && video->mpv == NULL && video->context == NULL) {
        return;
    }

    if(video->render_context != NULL) {
        mpv_render_context_set_update_callback(video->render_context, NULL, NULL);
        mpv_render_context_free(video->render_context);
    }
    if(video->mpv != NULL) {
        mpv_terminate_destroy(video->mpv);
    }
    if(video->context != NULL) {
        SDL_GL_DestroyContext(video->context);
    }

    uint32_t update_event_type = video->update_event_type;
    *video = (wd_video_state){.update_event_type = update_event_type};
}

#else

#include "error.h"

bool wd_run_video(struct wd_state* state) {
    (void)state;
    wd_set_error("video wallpaper support is disabled, rebuild with -DWD_VIDEO=ON");
    return false;
}

void wd_free_video(wd_video_state* video) {
    (void)video;
}

#endif
