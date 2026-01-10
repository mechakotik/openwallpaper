#ifndef AUDIO_VISUALIZER_H
#define AUDIO_VISUALIZER_H

#include <stdbool.h>
#include <stddef.h>
#include "argparse.h"

#ifndef WD_AUDIO_VISUALIZER
#define WD_AUDIO_VISUALIZER 0
#endif

#if WD_AUDIO_VISUALIZER
#include <pthread.h>
#include "input/common.h"
struct cava_plan;
#endif

typedef enum {
    WD_AUDIO_BACKEND_NONE,
    WD_AUDIO_BACKEND_PORTAUDIO,
    WD_AUDIO_BACKEND_PIPEWIRE,
    WD_AUDIO_BACKEND_PULSE,
} wd_audio_backend;

typedef struct wd_audio_visualizer_state {
    bool allowed;
#if WD_AUDIO_VISUALIZER
    wd_audio_backend custom_backend;
    const char* custom_source;
    bool initialized;
    bool failed;
    bool thread_running;
    size_t bars;
    size_t output_channels;
    size_t output_size;
    wd_audio_backend backend;
    struct audio_data audio;
    struct cava_plan* plan;
    double* output_buffer;
    pthread_t thread;
    bool mutex_initialized;
#endif
} wd_audio_visualizer_state;

bool wd_init_audio_visualizer(wd_audio_visualizer_state* state, wd_args_state* args);
void wd_free_audio_visualizer(wd_audio_visualizer_state* state);
void wd_audio_visualizer_get_spectrum(wd_audio_visualizer_state* state, float* data, size_t length);

#endif
