#ifndef WD_ERROR_H
#define WD_ERROR_H

#include <stdio.h>
#include <stdlib.h>

#define WD_LOG(...)      \
    printf(__VA_ARGS__); \
    printf("\n");

#define WD_WARN(...)     \
    printf(__VA_ARGS__); \
    printf("\n");

#define WD_ERROR(...)    \
    printf("error: ");   \
    printf(__VA_ARGS__); \
    printf("\n");        \
    exit(1);

#endif
