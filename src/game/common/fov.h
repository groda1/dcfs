#ifndef FOV_H
#define FOV_H

#include "core.h"
#include "level.h"
#include "memory_arena.h"

#define FOV_RADIUS 6

typedef struct
{
    i8 dx;
    i8 dy;
    u32 end;
} fov_cell_t;

typedef struct
{
    i32 radius;
    u32 cell_count;
    fov_cell_t *cells;
} fov_t;

void Fov_Init(fov_t *fov, arena_t *arena, i32 radius);
void Fov_Compute(const fov_t *fov, level_t *level, i32 origin_x, i32 origin_y);
bool Fov_TouchesVisibleOpenTile(const level_t *level, i32 x, i32 y);

#endif
