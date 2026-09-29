#ifndef DEBUG_CATEGORY_H
#define DEBUG_CATEGORY_H

#define GAME_DEBUG_CATEGORY_FIRST 64

typedef enum
{
    PROTOCOL = GAME_DEBUG_CATEGORY_FIRST,
    CLIENT,
    SERVER,
    LOS,
    LEVEL_GEN,
} game_debug_category_t;

#endif
