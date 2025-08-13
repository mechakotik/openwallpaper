#ifndef WD_ERROR_H
#define WD_ERROR_H

void wd_set_error(const char* format, ...);
const char* wd_get_last_error();

#endif
