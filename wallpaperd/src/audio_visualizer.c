#include "audio_visualizer.h"
#include <stdlib.h>
#include <string.h>
#include "error.h"

#ifndef WD_AUDIO_VISUALIZER
#define WD_AUDIO_VISUALIZER 0
#endif

static void fill_zeros(float* data, size_t length) {
    if(data != NULL && length > 0) {
        memset(data, 0, sizeof(float) * length);
    }
}

#if !WD_AUDIO_VISUALIZER

bool wd_init_audio_visualizer(wd_audio_visualizer_state* state, wd_args_state* args) {
    return true;
}

void wd_free_audio_visualizer(wd_audio_visualizer_state* state) {
    (void)state;
}

void wd_audio_visualizer_get_spectrum(wd_audio_visualizer_state* state, float* data, size_t length) {
    (void)state;
    fill_zeros(data, length);
}

#else

#include <limits.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include "cavacore.h"
#include "dynamic_api.h"
#include "input/common.h"

#ifdef WD_PIPEWIRE
#include "input/pipewire.h"
#endif
#ifdef WD_PULSE
#include "input/pulse.h"
#endif
#ifdef WD_PORTAUDIO
#include "input/portaudio.h"
#endif

static wd_audio_backend parse_backend(const char* name) {
    if(name == NULL) {
        return WD_AUDIO_BACKEND_NONE;
    }
    if(strcasecmp(name, "portaudio") == 0) {
        return WD_AUDIO_BACKEND_PORTAUDIO;
    }
    if(strcasecmp(name, "pipewire") == 0) {
        return WD_AUDIO_BACKEND_PIPEWIRE;
    }
    if(strcasecmp(name, "pulse") == 0) {
        return WD_AUDIO_BACKEND_PULSE;
    }
    return WD_AUDIO_BACKEND_NONE;
}

static const char* backend_name(wd_audio_backend backend) {
    switch(backend) {
        case WD_AUDIO_BACKEND_PORTAUDIO:
            return "portaudio";
        case WD_AUDIO_BACKEND_PIPEWIRE:
            return "pipewire";
        case WD_AUDIO_BACKEND_PULSE:
            return "pulse";
        default:
            return "unknown";
    }
}

static bool load_backend_library(wd_audio_backend backend) {
    switch(backend) {
#ifdef WD_PIPEWIRE
        case WD_AUDIO_BACKEND_PIPEWIRE:
            return wd_dynapi_load_pipewire();
#endif
#ifdef WD_PULSE
        case WD_AUDIO_BACKEND_PULSE:
            return wd_dynapi_load_pulse();
#endif
#ifdef WD_PORTAUDIO
        case WD_AUDIO_BACKEND_PORTAUDIO:
            return wd_dynapi_load_portaudio();
#endif
        default:
            return false;
    }
}

static const wd_audio_backend default_backends[] = {
#ifdef WD_PIPEWIRE
    WD_AUDIO_BACKEND_PIPEWIRE,
#endif
#ifdef WD_PULSE
    WD_AUDIO_BACKEND_PULSE,
#endif
#ifdef WD_PORTAUDIO
    WD_AUDIO_BACKEND_PORTAUDIO,
#endif
};

bool wd_init_audio_visualizer(wd_audio_visualizer_state* state, wd_args_state* args) {
    if(!WD_AUDIO_VISUALIZER) {
        return true;
    }
    state->allowed = (wd_get_option(args, "no-audio") == NULL);
    if(!state->allowed) {
        return true;
    }

    const char* custom_backend = wd_get_option(args, "audio-backend");
    if(custom_backend != NULL) {
        wd_audio_backend parsed = parse_backend(custom_backend);
        if(parsed == WD_AUDIO_BACKEND_NONE) {
            wd_set_error("unknown audio backend %s", custom_backend);
            return false;
        }
        if(!load_backend_library(parsed)) {
            wd_set_error("audio backend %s is not available", custom_backend);
            return false;
        }
        state->custom_backend = parsed;
    }
    state->custom_source = wd_get_option(args, "audio-source");

    return true;
}

void wd_free_audio_visualizer(wd_audio_visualizer_state* state) {
    if(state->thread_running) {
        state->audio.terminate = 1;
        pthread_join(state->thread, NULL);
        state->thread_running = false;
    }
    if(state->plan != NULL) {
        cava_destroy(state->plan);
        state->plan = NULL;
    }
    free(state->output_buffer);
    state->output_buffer = NULL;
    if(state->audio.cava_in != NULL) {
        free(state->audio.cava_in);
        state->audio.cava_in = NULL;
    }
    if(state->audio.source != NULL) {
        free(state->audio.source);
        state->audio.source = NULL;
    }
    if(state->mutex_initialized) {
        pthread_mutex_destroy(&state->audio.lock);
        state->mutex_initialized = false;
    }
    memset(&state->audio, 0, sizeof(state->audio));
    state->initialized = false;
    state->bars = 0;
    state->output_channels = 0;
    state->output_size = 0;
    state->backend = WD_AUDIO_BACKEND_NONE;
}

static void* (*backend_thread(wd_audio_backend backend))(void*) {
    switch(backend) {
#ifdef WD_PIPEWIRE
        case WD_AUDIO_BACKEND_PIPEWIRE:
            return input_pipewire;
#endif
#ifdef WD_PULSE
        case WD_AUDIO_BACKEND_PULSE:
            return input_pulse;
#endif
#ifdef WD_PORTAUDIO
        case WD_AUDIO_BACKEND_PORTAUDIO:
            return input_portaudio;
#endif
        default:
            return NULL;
    }
}

static bool wait_for_ready(wd_audio_visualizer_state* state) {
    const struct timespec delay = {.tv_sec = 0, .tv_nsec = 1000000};
    for(int i = 0; i < 5000; i++) {
        pthread_mutex_lock(&state->audio.lock);
        int format = state->audio.format;
        unsigned int rate = state->audio.rate;
        int threadparams = state->audio.threadparams;
        int channels = state->audio.channels;
        int terminate = state->audio.terminate;
        pthread_mutex_unlock(&state->audio.lock);

        if(threadparams == 0 && format != -1 && rate != 0 && channels > 0) {
            return true;
        }
        if(terminate) {
            return false;
        }
        nanosleep(&delay, NULL);
    }
    return false;
}

static bool recreate_plan(wd_audio_visualizer_state* state, size_t length) {
    int channels = state->audio.channels;
    if(channels < 1) {
        channels = 1;
    }
    if(channels > 2) {
        channels = 2;
    }
    if(length == 0) {
        return false;
    }
    size_t bars_size_t = length;
    if(bars_size_t > (size_t)INT_MAX) {
        bars_size_t = (size_t)INT_MAX;
    }
    int number_of_bars = (int)bars_size_t;
    struct cava_plan* plan = cava_init(number_of_bars, state->audio.rate, channels, 1, 0.77, 50, 10000);
    if(plan == NULL || plan->status != 0) {
        wd_set_error("cava init failed");
        if(plan != NULL) {
            cava_destroy(plan);
        }
        return false;
    }

    pthread_mutex_lock(&state->audio.lock);
    if(plan->input_buffer_size != state->audio.cava_buffer_size) {
        state->audio.cava_buffer_size = plan->input_buffer_size;
        state->audio.cava_in = realloc(state->audio.cava_in, sizeof(double) * state->audio.cava_buffer_size);
    }
    state->audio.samples_counter = 0;
    pthread_mutex_unlock(&state->audio.lock);

    free(state->output_buffer);
    state->output_buffer = calloc((size_t)number_of_bars * channels, sizeof(double));
    if(state->plan != NULL) {
        cava_destroy(state->plan);
    }

    state->plan = plan;
    state->bars = (size_t)number_of_bars;
    state->output_channels = (size_t)channels;
    state->output_size = state->bars * state->output_channels;
    return true;
}

static bool start_backend(
    wd_audio_visualizer_state* state, wd_audio_backend backend, const char* source, size_t length) {
    if(!load_backend_library(backend)) {
        wd_set_error("%s backend is not available", backend_name(backend));
        return false;
    }

    wd_free_audio_visualizer(state);
    state->backend = backend;

    state->audio = (struct audio_data){
        .source = strdup(source != NULL ? source : "auto"),
        .format = -1,
        .rate = 0,
        .channels = 2,
        .IEEE_FLOAT = 0,
        .autoconnect = 0,
        .active = 1,
        .remix = 1,
        .virtual_node = 1,
        .cava_buffer_size = 16384,
    };

    state->audio.cava_in = calloc(state->audio.cava_buffer_size, sizeof(double));

    switch(backend) {
        case WD_AUDIO_BACKEND_PORTAUDIO:
            state->audio.rate = 44100;
            state->audio.format = 16;
            state->audio.threadparams = 1;
            break;
        case WD_AUDIO_BACKEND_PIPEWIRE:
            state->audio.rate = 48000;
            state->audio.format = 16;
            break;
        case WD_AUDIO_BACKEND_PULSE:
            state->audio.rate = 44100;
            state->audio.format = 16;
            break;
        default:
            break;
    }

    state->audio.input_buffer_size = BUFFER_SIZE * state->audio.channels;
    if(state->audio.cava_buffer_size < state->audio.input_buffer_size * 8) {
        state->audio.cava_buffer_size = state->audio.input_buffer_size * 8;
        state->audio.cava_in = realloc(state->audio.cava_in, sizeof(double) * state->audio.cava_buffer_size);
    }

    if(pthread_mutex_init(&state->audio.lock, NULL) != 0) {
        wd_set_error("could not initialize audio lock");
        goto handle_error;
    }
    state->mutex_initialized = true;

    void* (*thread_fn)(void*) = backend_thread(backend);
    if(thread_fn == NULL) {
        wd_set_error("no available audio backend thread");
        goto handle_error;
    }

    if(pthread_create(&state->thread, NULL, thread_fn, (void*)&state->audio) != 0) {
        wd_set_error("could not create audio backend thread");
        goto handle_error;
    }
    state->thread_running = true;

    if(!wait_for_ready(state)) {
        wd_set_error("audio backend thread hung");
        goto handle_error;
    }
    if(!recreate_plan(state, length)) {
        goto handle_error;
    }

    state->initialized = true;
    state->failed = false;
    return true;

handle_error:
    wd_free_audio_visualizer(state);
    return false;
}

static bool init_visualizer(wd_audio_visualizer_state* state, size_t length) {
    if(state->custom_backend != WD_AUDIO_BACKEND_NONE) {
        return start_backend(state, state->custom_backend, state->custom_source, length);
    }

    int backend_count = sizeof(default_backends) / sizeof(default_backends[0]);
    for(int i = 0; i < backend_count; i++) {
        wd_audio_backend backend = default_backends[i];
        if(!load_backend_library(backend)) {
            continue;
        }
        if(start_backend(state, backend, state->custom_source, length)) {
            return true;
        }
    }
    return false;
}

void wd_audio_visualizer_get_spectrum(wd_audio_visualizer_state* state, float* data, size_t length) {
    if(length == 0) {
        return;
    }
    if(!state->allowed || data == NULL) {
        fill_zeros(data, length);
        return;
    }

    if(!state->initialized && !state->failed) {
        if(!init_visualizer(state, length)) {
            state->failed = true;
        }
    }

    if(!state->initialized) {
        fill_zeros(data, length);
        return;
    }

    if(state->bars != length) {
        if(!recreate_plan(state, length)) {
            fill_zeros(data, length);
            return;
        }
    }

    pthread_mutex_lock(&state->audio.lock);
    cava_execute(state->audio.cava_in, state->audio.samples_counter, state->output_buffer, state->plan);
    state->audio.samples_counter = 0;
    pthread_mutex_unlock(&state->audio.lock);

    size_t bars = state->bars;
    if(bars > length) {
        bars = length;
    }
    for(size_t i = 0; i < bars; i++) {
        double value = 0.0;
        for(size_t ch = 0; ch < state->output_channels; ch++) {
            size_t idx = i + ch * state->bars;
            if(idx < state->output_size && state->output_buffer[idx] > value) {
                value = state->output_buffer[idx];
            }
        }
        if(value < 0.0) {
            value = 0.0;
        } else if(value > 1.0) {
            value = 1.0;
        }
        data[i] = (float)value;
    }
    if(bars < length) {
        fill_zeros(data + bars, length - bars);
    }
}

#endif
