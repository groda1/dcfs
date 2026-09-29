#ifndef LOS_H
#define LOS_H

#include "core.h"
#include "level.h"
#include "memory_arena.h"

#define LOS_RADIUS 6

typedef struct
{
    i8 dx;
    i8 dy;
    u32 end;
} los_cell_t;

typedef struct
{
    i32 radius;
    u32 cell_count;
    los_cell_t *cells;
} los_t;

void LoS_Init(los_t *los, arena_t *arena, i32 radius);
void LoS_Compute(const los_t *los, level_t *level, i32 origin_x, i32 origin_y);
bool LoS_TouchesVisibleOpenTile(const level_t *level, i32 x, i32 y);

#endif
