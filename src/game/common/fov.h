#ifndef FOV_H
#define FOV_H

#include "core.h"
#include "level.h"

#define FOV_RADIUS 7

void Fov_Compute(level_t *level, i32 origin_x, i32 origin_y, i32 radius);
bool Fov_TouchesVisibleOpenTile(const level_t *level, i32 x, i32 y);

static inline bool Fov_InRadius(i32 dx, i32 dy, i32 radius)
{
    return dx * dx + dy * dy <= radius * radius + radius;
}

#endif
