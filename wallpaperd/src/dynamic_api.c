#include "dynamic_api.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>

#define DECLARE(ret, name, args, call_args) \
    static ret(*impl_##name) args;          \
    ret name args {                         \
        return impl_##name call_args;       \
    }

#define IMPORT(name)                     \
    impl_##name = dlsym(wayland, #name); \
    if(dlerror() != NULL) {              \
        dlclose(wayland);                \
        wayland = NULL;                  \
        return false;                    \
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
DECLARE(void, wl_proxy_destroy, (struct wl_proxy * proxy), (proxy));
DECLARE(void, wl_display_disconnect, (struct wl_display * display), (display));

static struct wl_proxy* (*impl_wl_proxy_marshal_flags)(struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, uint32_t version, uint32_t flags, ...);

__attribute__((naked)) struct wl_proxy* wl_proxy_marshal_flags(struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, uint32_t version, uint32_t flags, ...) {
    __asm__ volatile("jmp *%0" : : "m"(impl_wl_proxy_marshal_flags));
}

bool wd_dynapi_load_wayland() {
    if(wayland != NULL) {
        return true;
    }

    wayland = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LAZY);
    if(wayland == NULL) {
        return false;
    }

    IMPORT(wl_proxy_marshal_flags);
    IMPORT(wl_proxy_get_version);
    IMPORT(wl_display_connect);
    IMPORT(wl_proxy_add_listener);
    IMPORT(wl_display_roundtrip);
    IMPORT(wl_proxy_destroy);
    IMPORT(wl_display_disconnect);

    return true;
}
