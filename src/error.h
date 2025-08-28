#ifndef WD_ERROR_H
#define WD_ERROR_H

#include <stdbool.h>

void wd_set_error(const char* format, ...);
void wd_set_scene_error(const char* format, ...);
const char* wd_get_last_error();
bool wd_is_last_error_scene_error();

#endif
