#ifndef WD_VIDEO_H
#define WD_VIDEO_H

#include <stdbool.h>

#ifdef WD_VIDEO

#include <SDL3/SDL_video.h>
#include <stdatomic.h>
#include <stdint.h>

struct wd_state;
typedef struct mpv_handle mpv_handle;
typedef struct mpv_render_context mpv_render_context;

typedef struct wd_video_state {
    SDL_GLContext context;
    mpv_handle* mpv;
    mpv_render_context* render_context;
    atomic_bool update_pending;
    uint32_t update_event_type;
    bool paused;
} wd_video_state;

#else

struct wd_state;

typedef struct wd_video_state {
} wd_video_state;

#endif

bool wd_run_video(struct wd_state* state);
void wd_free_video(wd_video_state* video);

#endif
