#ifndef RULES_H
#define RULES_H

#include "core.h"
#include "level.h"

bool Rules_IsWalkable(const level_t *level, i32 x, i32 y);
bool Rules_BlocksSight(const level_t *level, i32 x, i32 y);

#endif
