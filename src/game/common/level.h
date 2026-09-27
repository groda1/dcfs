#ifndef LEVEL_H
#define LEVEL_H

#include "core.h"
#include "memory_arena.h"

typedef enum
{
    TILE_EMPTY,
    TILE_FLOOR,
    TILE_WALL,
} tile_type_t;

typedef enum
{
    FLAG_REVEALED = 1 << 0,
} tile_flag_t;

typedef struct
{
    u16 type;
    u16 flags;
} tile_t;

typedef struct
{
    u16 width;
    u16 height;
    tile_t *tiles;
} level_t;

void Level_Init(level_t *level, arena_t *arena, u16 width, u16 height);

static inline bool Level_InBounds(const level_t *level, i32 x, i32 y)
{
    return x >= 0 && x < level->width && y >= 0 && y < level->height;
}

static inline tile_t *Level_GetTile(const level_t *level, i32 x, i32 y)
{
    Assert(Level_InBounds(level, x, y));
    return &level->tiles[y * level->width + x];
}

#endif
