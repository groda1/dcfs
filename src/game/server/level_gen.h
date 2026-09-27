#ifndef LEVEL_GEN_H
#define LEVEL_GEN_H

#include "core.h"
#include "level.h"
#include "rng.h"

void LevelGen_Generate(level_t *level, rng_t *rng, i32 *start_x, i32 *start_y);

#endif
