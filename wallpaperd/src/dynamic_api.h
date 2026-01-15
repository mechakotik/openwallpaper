#ifndef WD_DYNAMIC_API_H
#define WD_DYNAMIC_API_H

#include <stdbool.h>

bool wd_dynapi_load_wayland();

#ifdef WD_PIPEWIRE
bool wd_dynapi_load_pipewire();
#endif

#ifdef WD_PULSE
bool wd_dynapi_load_pulse();
#endif

#ifdef WD_PORTAUDIO
bool wd_dynapi_load_portaudio();
#endif

#endif
