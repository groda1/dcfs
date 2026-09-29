#ifndef SIGHT_H
#define SIGHT_H

#include "core.h"
#include "core_math.h"
#include "level.h"

#define SIGHT_MAX_TILE_INSTANCES      5

typedef struct
{
    mat4 transform;
    u32 flags;
    u32 pad[3];
} sight_mask_instance_t;
StaticAssert(sizeof(sight_mask_instance_t) == 80, "sight_mask_instance_t must match the shader's std430 stride");

void Sight_Init(f32 wall_height);
void Sight_Reset(const level_t *level);
u32  Sight_WriteInstances(i32 x, i32 y, vec3 center, sight_mask_instance_t *instances);

#endif
