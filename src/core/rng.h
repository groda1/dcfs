#ifndef RNG_H
#define RNG_H

#include "core.h"

typedef struct
{
    u64 state;
} rng_t;

void Rng_Seed(rng_t *rng, u64 seed);
u64  Rng_U64(rng_t *rng);
i32  Rng_Range(rng_t *rng, i32 min, i32 max_exclusive);

#endif
