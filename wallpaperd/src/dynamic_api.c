#include "dynamic_api.h"
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void* dynapi_dlopen(const char** names, size_t count) {
    for(size_t i = 0; i < count; i++) {
        void* handle = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if(handle != NULL) {
            dlerror();
            return handle;
        }
    }
    return NULL;
}

static void dynapi_dlclose(void** handle) {
    if(*handle != NULL) {
        dlclose(*handle);
        *handle = NULL;
    }
}

static bool dynapi_import(void* handle, const char* name, void** fn) {
    dlerror();
    *fn = dlsym(handle, name);
    return *fn != NULL && dlerror() == NULL;
}

// NOTE:
// __attribute__((visibility("hidden"))) marks exported symbols as "local",
// signaling the dynamic linker that only *this* binary (shared object) can
// resolve them, including the statically linked CAVA, while allowing
// dynamically linked SDL to resolve to symbols from its explicitly linked .so
// dependencies instead of our symbols, since they can remain uninitialized.
// Also see: https://gcc.gnu.org/onlinedocs/gcc/Common-Attributes.html#index-visibility

#define DECLARE(ret, name, args, call_args)               \
    static ret(*impl_##name) args;                        \
    __attribute__((visibility("hidden"))) ret name args { \
        return impl_##name call_args;                     \
    }

#define DECLARE_VOID(name, args, call_args)                \
    static void(*impl_##name) args;                        \
    __attribute__((visibility("hidden"))) void name args { \
        impl_##name call_args;                             \
    }

#define IMPORT(handle, name, cleanup)                         \
    if(!dynapi_import(handle, #name, (void**)&impl_##name)) { \
        cleanup;                                              \
        return false;                                         \
    }

static void* wayland;

struct wl_proxy;
struct wl_interface;
struct wl_display;

DECLARE(uint32_t, wl_proxy_get_version, (struct wl_proxy * proxy), (proxy));
DECLARE(struct wl_display*, wl_display_connect, (const char* name), (name));
DECLARE(int, wl_proxy_add_listener, (struct wl_proxy * proxy, void (**implementation)(void), void* data),
    (proxy, implementation, data));
DECLARE(int, wl_display_roundtrip, (struct wl_display * display), (display));
DECLARE_VOID(wl_proxy_destroy, (struct wl_proxy * proxy), (proxy));
DECLARE_VOID(wl_display_disconnect, (struct wl_display * display), (display));

static struct wl_proxy* (*impl_wl_proxy_marshal_flags)(struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, uint32_t version, uint32_t flags, ...);

__attribute__((visibility("hidden"))) __attribute__((naked)) struct wl_proxy* wl_proxy_marshal_flags(
    struct wl_proxy* proxy, uint32_t opcode, const struct wl_interface* interface, uint32_t version, uint32_t flags,
    ...) {
    __asm__ volatile("jmp *%0" : : "m"(impl_wl_proxy_marshal_flags));
}

bool wd_dynapi_load_wayland() {
    if(wayland != NULL) {
        return true;
    }

    static const char* libraries[] = {"libwayland-client.so.0", "libwayland-client.so"};
    wayland = dynapi_dlopen(libraries, sizeof(libraries) / sizeof(libraries[0]));
    if(wayland == NULL) {
        return false;
    }

    IMPORT(wayland, wl_proxy_marshal_flags, dynapi_dlclose(&wayland));
    IMPORT(wayland, wl_proxy_get_version, dynapi_dlclose(&wayland));
    IMPORT(wayland, wl_display_connect, dynapi_dlclose(&wayland));
    IMPORT(wayland, wl_proxy_add_listener, dynapi_dlclose(&wayland));
    IMPORT(wayland, wl_display_roundtrip, dynapi_dlclose(&wayland));
    IMPORT(wayland, wl_proxy_destroy, dynapi_dlclose(&wayland));
    IMPORT(wayland, wl_display_disconnect, dynapi_dlclose(&wayland));

    return true;
}

#ifdef WD_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/utils/dict.h>

enum spa_log_level pw_log_level = SPA_LOG_LEVEL_WARN;
struct spa_log_topic* const PW_LOG_TOPIC_DEFAULT = NULL;

static void* pipewire;

static void wd_dynapi_close_pipewire() {
    dynapi_dlclose(&pipewire);
}

DECLARE_VOID(pw_init, (int* argc, char*** argv), (argc, argv));
DECLARE_VOID(pw_deinit, (void), ());
DECLARE(struct pw_main_loop*, pw_main_loop_new, (const struct spa_dict* props), (props));
DECLARE(struct pw_loop*, pw_main_loop_get_loop, (struct pw_main_loop * loop), (loop));
DECLARE_VOID(pw_main_loop_destroy, (struct pw_main_loop * loop), (loop));
DECLARE(int, pw_main_loop_run, (struct pw_main_loop * loop), (loop));
DECLARE(int, pw_main_loop_quit, (struct pw_main_loop * loop), (loop));
DECLARE(struct pw_stream*, pw_stream_new_simple,
    (struct pw_loop * loop, const char* name, struct pw_properties* props, const struct pw_stream_events* events,
        void* data),
    (loop, name, props, events, data));
DECLARE(int, pw_stream_connect,
    (struct pw_stream * stream, enum pw_direction direction, uint32_t target_id, enum pw_stream_flags flags,
        const struct spa_pod** params, uint32_t n_params),
    (stream, direction, target_id, flags, params, n_params));
DECLARE(struct pw_buffer*, pw_stream_dequeue_buffer, (struct pw_stream * stream), (stream));
DECLARE(int, pw_stream_queue_buffer, (struct pw_stream * stream, struct pw_buffer* buffer), (stream, buffer));
DECLARE_VOID(pw_stream_destroy, (struct pw_stream * stream), (stream));
DECLARE(int, pw_properties_set, (struct pw_properties * properties, const char* key, const char* value),
    (properties, key, value));

static struct pw_properties* (*impl_pw_properties_new)(const char*, ...);
static int (*impl_pw_properties_setf)(struct pw_properties*, const char*, const char*, ...);
static void (*impl_pw_log_logt)(enum spa_log_level level, const struct spa_log_topic* topic, const char* file, int line,
    const char* func, const char* fmt, ...);

__attribute__((visibility("hidden"))) __attribute__((naked)) struct pw_properties* pw_properties_new(
    const char* key, ...) {
    __asm__ volatile("jmp *%0" : : "m"(impl_pw_properties_new));
}

__attribute__((visibility("hidden"))) __attribute__((naked)) int pw_properties_setf(
    struct pw_properties* properties, const char* key, const char* format, ...) {
    __asm__ volatile("jmp *%0" : : "m"(impl_pw_properties_setf));
}

__attribute__((visibility("hidden"))) __attribute__((naked)) void pw_log_logt(enum spa_log_level level,
    const struct spa_log_topic* topic, const char* file, int line, const char* func, const char* fmt, ...) {
    __asm__ volatile("jmp *%0" : : "m"(impl_pw_log_logt));
}

bool wd_dynapi_load_pipewire() {
    if(pipewire != NULL) {
        return true;
    }

    static const char* libraries[] = {"libpipewire-0.3.so.0", "libpipewire-0.3.so"};
    pipewire = dynapi_dlopen(libraries, sizeof(libraries) / sizeof(libraries[0]));
    if(pipewire == NULL) {
        return false;
    }

    IMPORT(pipewire, pw_init, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_deinit, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_main_loop_new, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_main_loop_get_loop, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_main_loop_destroy, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_main_loop_run, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_main_loop_quit, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_stream_new_simple, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_stream_connect, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_stream_dequeue_buffer, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_stream_queue_buffer, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_stream_destroy, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_properties_new, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_properties_set, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_properties_setf, wd_dynapi_close_pipewire());
    IMPORT(pipewire, pw_log_logt, wd_dynapi_close_pipewire());

    return true;
}
#endif

#ifdef WD_PULSE
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

static void* pulse;
static void* pulse_simple;

static void wd_dynapi_close_pulse() {
    dynapi_dlclose(&pulse);
    dynapi_dlclose(&pulse_simple);
}

DECLARE(pa_mainloop*, pa_mainloop_new, (void), ());
DECLARE(pa_mainloop_api*, pa_mainloop_get_api, (pa_mainloop * m), (m));
DECLARE(pa_context*, pa_context_new, (pa_mainloop_api * api, const char* name), (api, name));
DECLARE(int, pa_context_connect,
    (pa_context * c, const char* server, pa_context_flags_t flags, const pa_spawn_api* api), (c, server, flags, api));
DECLARE_VOID(
    pa_context_set_state_callback, (pa_context * c, pa_context_notify_cb_t cb, void* userdata), (c, cb, userdata));
DECLARE(int, pa_mainloop_iterate, (pa_mainloop * m, int block, int* retval), (m, block, retval));
DECLARE(int, pa_mainloop_run, (pa_mainloop * m, int* retval), (m, retval));
DECLARE_VOID(pa_mainloop_free, (pa_mainloop * m), (m));
DECLARE(pa_simple*, pa_simple_new,
    (const char* server, const char* name, pa_stream_direction_t dir, const char* dev, const char* stream_name,
        const pa_sample_spec* ss, const pa_channel_map* map, const pa_buffer_attr* attr, int* error),
    (server, name, dir, dev, stream_name, ss, map, attr, error));
DECLARE(int, pa_simple_read, (pa_simple * s, void* data, size_t bytes, int* error), (s, data, bytes, error));
DECLARE_VOID(pa_simple_free, (pa_simple * s), (s));
DECLARE(pa_context_state_t, pa_context_get_state, (const pa_context* c), (c));
DECLARE_VOID(pa_operation_unref, (pa_operation * o), (o));
DECLARE(pa_operation*, pa_context_get_server_info, (pa_context * c, pa_server_info_cb_t cb, void* userdata),
    (c, cb, userdata));
DECLARE_VOID(pa_context_disconnect, (pa_context * c), (c));
DECLARE_VOID(pa_context_unref, (pa_context * c), (c));
DECLARE_VOID(pa_mainloop_quit, (pa_mainloop * m, int retval), (m, retval));
DECLARE(const char*, pa_strerror, (int error), (error));

bool wd_dynapi_load_pulse() {
    if(pulse != NULL && pulse_simple != NULL) {
        return true;
    }

    static const char* pulse_libs[] = {"libpulse.so.0", "libpulse.so"};
    static const char* pulse_simple_libs[] = {"libpulse-simple.so.0", "libpulse-simple.so"};
    pulse = dynapi_dlopen(pulse_libs, sizeof(pulse_libs) / sizeof(pulse_libs[0]));
    pulse_simple = dynapi_dlopen(pulse_simple_libs, sizeof(pulse_simple_libs) / sizeof(pulse_simple_libs[0]));

    if(pulse == NULL || pulse_simple == NULL) {
        wd_dynapi_close_pulse();
        return false;
    }

    IMPORT(pulse, pa_mainloop_new, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_mainloop_get_api, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_context_new, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_context_connect, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_context_set_state_callback, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_mainloop_iterate, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_mainloop_run, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_mainloop_free, wd_dynapi_close_pulse());
    IMPORT(pulse_simple, pa_simple_new, wd_dynapi_close_pulse());
    IMPORT(pulse_simple, pa_simple_read, wd_dynapi_close_pulse());
    IMPORT(pulse_simple, pa_simple_free, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_context_get_state, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_operation_unref, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_context_get_server_info, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_context_disconnect, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_context_unref, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_mainloop_quit, wd_dynapi_close_pulse());
    IMPORT(pulse, pa_strerror, wd_dynapi_close_pulse());

    return true;
}
#endif

#ifdef WD_PORTAUDIO
#include <portaudio.h>

static void* portaudio;

static void wd_dynapi_close_portaudio() {
    dynapi_dlclose(&portaudio);
}

DECLARE(PaError, Pa_Initialize, (void), ());
DECLARE(PaError, Pa_Terminate, (void), ());
DECLARE(const char*, Pa_GetErrorText, (PaError errorCode), (errorCode));
DECLARE(PaDeviceIndex, Pa_GetDeviceCount, (void), ());
DECLARE(const PaDeviceInfo*, Pa_GetDeviceInfo, (PaDeviceIndex device), (device));
DECLARE(PaDeviceIndex, Pa_GetDefaultInputDevice, (void), ());
DECLARE(PaError, Pa_IsFormatSupported,
    (const PaStreamParameters* inputParameters, const PaStreamParameters* outputParameters, double sampleRate),
    (inputParameters, outputParameters, sampleRate));
DECLARE(PaError, Pa_OpenStream,
    (PaStream * *stream, const PaStreamParameters* inputParameters, const PaStreamParameters* outputParameters,
        double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback* streamCallback,
        void* userData),
    (stream, inputParameters, outputParameters, sampleRate, framesPerBuffer, streamFlags, streamCallback, userData));
DECLARE(PaError, Pa_StartStream, (PaStream * stream), (stream));
DECLARE(PaError, Pa_IsStreamActive, (PaStream * stream), (stream));
DECLARE_VOID(Pa_Sleep, (long msec), (msec));
DECLARE(PaError, Pa_CloseStream, (PaStream * stream), (stream));

bool wd_dynapi_load_portaudio() {
    if(portaudio != NULL) {
        return true;
    }

    static const char* libraries[] = {"libportaudio.so.2", "libportaudio.so"};
    portaudio = dynapi_dlopen(libraries, sizeof(libraries) / sizeof(libraries[0]));
    if(portaudio == NULL) {
        return false;
    }

    IMPORT(portaudio, Pa_Initialize, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_Terminate, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_GetErrorText, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_GetDeviceCount, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_GetDeviceInfo, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_GetDefaultInputDevice, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_IsFormatSupported, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_OpenStream, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_StartStream, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_IsStreamActive, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_Sleep, wd_dynapi_close_portaudio());
    IMPORT(portaudio, Pa_CloseStream, wd_dynapi_close_portaudio());

    return true;
}
#endif
