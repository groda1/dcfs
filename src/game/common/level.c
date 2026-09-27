#include "level.h"

void Level_Init(level_t *level, arena_t *arena, u16 width, u16 height)
{
    level->width = width;
    level->height = height;
    level->tiles = arena_push_array(arena, tile_t, (u64)width * height);
}
