#include "rng.h"

void Rng_Seed(rng_t *rng, u64 seed)
{
    u64 z = seed + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z = z ^ (z >> 31);

    rng->state = z ? z : 1;
}

u64 Rng_U64(rng_t *rng)
{
    u64 x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;

    return x * 0x2545F4914F6CDD1Dull;
}

i32 Rng_Range(rng_t *rng, i32 min, i32 max_exclusive)
{
    Assert(max_exclusive > min);

    u64 span = (u64)((i64)max_exclusive - (i64)min);
    return (i32)((i64)min + (i64)(Rng_U64(rng) % span));
}
