#include "rules.h"

bool Rules_IsWalkable(const level_t *level, i32 x, i32 y)
{
    if (!Level_InBounds(level, x, y))
        return false;

    return Level_GetTile(level, x, y)->type == TILE_FLOOR;
}
