#ifndef FOG_H
#define FOG_H

#include "core.h"
#include "core_math.h"
#include "level.h"
#include "memory_arena.h"

#define FOG_TEAR_DEPTH              0.25f
#define FOG_EDGE_SOFTNESS           0.12f
#define FOG_SPEED                   10.0f

#define FOG_SURFACE_SAMPLES         9
#define FOG_EDGE_SAMPLES            16
#define FOG_MAX_TILE_INSTANCES      5

typedef struct
{
    mat4 transform;
    u32 boxes;
    u32 flags;
    f32 surface[FOG_SURFACE_SAMPLES];
    f32 edge[FOG_EDGE_SAMPLES];
    f32 known_surface[FOG_SURFACE_SAMPLES];
    f32 known_edge[FOG_EDGE_SAMPLES];
} fog_mask_instance_t;
StaticAssert(sizeof(fog_mask_instance_t) == 272, "fog_mask_instance_t must match the shader's std430 stride");

void Fog_Init(arena_t *arena, f32 wall_height);
void Fog_Reset(level_t *level, arena_t *run_arena);
void Fog_Snapshot(void);
void Fog_OnVisibilityChanged(i32 origin_x, i32 origin_y);
void Fog_Update(f32 delta_time);
u32  Fog_WriteInstances(i32 x, i32 y, vec3 center, fog_mask_instance_t *instances);

#endif
