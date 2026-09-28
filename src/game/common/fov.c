#include "fov.h"
#include "rules.h"

static bool line_clear(const level_t *level, i32 x0, i32 y0, i32 x1, i32 y1);
static bool faces_visible_open_tile(const level_t *level, i32 x, i32 y, i32 origin_x, i32 origin_y);

void Fov_Compute(level_t *level, i32 origin_x, i32 origin_y, i32 radius)
{
    u64 tile_count = (u64)level->width * level->height;
    for (u64 i = 0; i < tile_count; i++)
        level->tiles[i].flags &= (u16)~FLAG_VISIBLE;

    for (i32 y = origin_y - radius; y <= origin_y + radius; y++)
    {
        for (i32 x = origin_x - radius; x <= origin_x + radius; x++)
        {
            if (!Level_InBounds(level, x, y))
                continue;

            if (!Fov_InRadius(x - origin_x, y - origin_y, radius))
                continue;

            if (line_clear(level, origin_x, origin_y, x, y)
                || line_clear(level, x, y, origin_x, origin_y))
            {
                Level_GetTile(level, x, y)->flags |= FLAG_VISIBLE;
            }
        }
    }

    for (i32 y = origin_y - radius; y <= origin_y + radius; y++)
    {
        for (i32 x = origin_x - radius; x <= origin_x + radius; x++)
        {
            if (!Level_InBounds(level, x, y) || !Fov_InRadius(x - origin_x, y - origin_y, radius))
                continue;

            tile_t *tile = Level_GetTile(level, x, y);
            if ((tile->flags & FLAG_VISIBLE) || !Rules_BlocksSight(level, x, y))
                continue;

            if (faces_visible_open_tile(level, x, y, origin_x, origin_y))
                tile->flags |= FLAG_VISIBLE;
        }
    }
}

bool Fov_TouchesVisibleOpenTile(const level_t *level, i32 x, i32 y)
{
    for (i32 ny = y - 1; ny <= y + 1; ny++)
    {
        for (i32 nx = x - 1; nx <= x + 1; nx++)
        {
            if (!Level_InBounds(level, nx, ny) || Rules_BlocksSight(level, nx, ny))
                continue;

            if (Level_GetTile(level, nx, ny)->flags & FLAG_VISIBLE)
                return true;
        }
    }

    return false;
}

static bool faces_visible_open_tile(const level_t *level, i32 x, i32 y, i32 origin_x, i32 origin_y)
{
    i32 toward_x = (origin_x > x) - (origin_x < x);
    i32 toward_y = (origin_y > y) - (origin_y < y);

    for (i32 dy = -1; dy <= 1; dy++)
    {
        for (i32 dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;

            if ((dx != 0 && dx == -toward_x) || (dy != 0 && dy == -toward_y))
                continue;

            if (dx != 0 && dy != 0
                && !(Rules_BlocksSight(level, x + dx, y) && Rules_BlocksSight(level, x, y + dy)))
                continue;

            if (Rules_BlocksSight(level, x + dx, y + dy))
                continue;

            if (Level_GetTile(level, x + dx, y + dy)->flags & FLAG_VISIBLE)
                return true;
        }
    }

    return false;
}

static bool line_clear(const level_t *level, i32 x0, i32 y0, i32 x1, i32 y1)
{
    i32 dx = x1 > x0 ? x1 - x0 : x0 - x1;
    i32 dy = y1 > y0 ? y1 - y0 : y0 - y1;
    i32 sx = x1 > x0 ? 1 : -1;
    i32 sy = y1 > y0 ? 1 : -1;
    i32 err = dx - dy;

    i32 x = x0;
    i32 y = y0;
    for (;;)
    {
        i32 e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y += sy;
        }

        if (x == x1 && y == y1)
            return true;

        if (Rules_BlocksSight(level, x, y))
            return false;
    }
}
