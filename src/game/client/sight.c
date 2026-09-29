#include "sight.h"

#define SIGHT_LIT_BIT                 0
#define SIGHT_HIDDEN_BIT              1
#define SIGHT_KNOWN_SHIFT             2

typedef enum
{
    SIGHT_KNOWN_DISSOLVE,
    SIGHT_KNOWN_FULL,
} sight_known_t;

typedef enum
{
    SIGHT_SIDE_S,
    SIGHT_SIDE_E,
    SIGHT_SIDE_N,
    SIGHT_SIDE_W,
    SIGHT_SIDE_COUNT,
} sight_side_t;

typedef struct
{
    const level_t *level;
    f32 wall_height;
} sight_t;

static sight_t g_sight;

static const i32 SIDE_DX[SIGHT_SIDE_COUNT] = { 0, 1, 0, -1 };
static const i32 SIDE_DY[SIGHT_SIDE_COUNT] = { -1, 0, 1, 0 };
static const f32 SIDE_YAW_DEG[SIGHT_SIDE_COUNT] = { 0.0f, 90.0f, 180.0f, -90.0f };

static bool tile_known(i32 x, i32 y);
static bool tile_wall(i32 x, i32 y);
static bool tile_visible(i32 x, i32 y);
static bool face_exists(i32 x, i32 y, sight_side_t side);
static bool surface_lit(i32 x, i32 y, u32 surface);
static void write_surface(i32 x, i32 y, u32 surface, mat4 transform, sight_mask_instance_t *instance);

void Sight_Init(f32 wall_height)
{
    g_sight.wall_height = wall_height;
}

void Sight_Reset(const level_t *level)
{
    g_sight.level = level;
}

u32 Sight_WriteInstances(i32 x, i32 y, vec3 center, sight_mask_instance_t *instances)
{
    f32 height = g_sight.wall_height;
    bool wall = tile_wall(x, y);
    u32 count = 0;

    vec3 top_center = center;
    if (wall)
        top_center.Y = height;

    mat4 flat = HMM_Rotate_RH(HMM_AngleDeg(-90.0f), V3(1.0f, 0.0f, 0.0f));
    write_surface(x, y, 0, HMM_MulM4(HMM_Translate(top_center), flat), &instances[count++]);

    if (!wall)
        return count;

    for (i32 side = 0; side < SIGHT_SIDE_COUNT; side++)
    {
        if (!face_exists(x, y, (sight_side_t)side))
            continue;

        vec3 side_center = V3(center.X + 0.5f * (f32)SIDE_DX[side],
                              0.5f * height,
                              center.Z - 0.5f * (f32)SIDE_DY[side]);
        mat4 transform = HMM_MulM4(
                            HMM_Translate(side_center),
                            HMM_MulM4(
                                HMM_Rotate_RH(HMM_AngleDeg(SIDE_YAW_DEG[side]), V3(0.0f, 1.0f, 0.0f)),
                                HMM_Scale(V3(1.0f, height, 1.0f))
                            )
                        );
        write_surface(x, y, 1 + (u32)side, transform, &instances[count++]);
    }

    return count;
}

static bool tile_known(i32 x, i32 y)
{
    const level_t *level = g_sight.level;
    if (!Level_InBounds(level, x, y))
        return false;

    const tile_t *tile = Level_GetTile(level, x, y);
    return (tile->flags & FLAG_REVEALED) && tile->type != TILE_EMPTY;
}

static bool tile_wall(i32 x, i32 y)
{
    return tile_known(x, y) && Level_GetTile(g_sight.level, x, y)->type == TILE_WALL;
}

static bool tile_visible(i32 x, i32 y)
{
    return tile_known(x, y) && (Level_GetTile(g_sight.level, x, y)->flags & FLAG_VISIBLE);
}

static bool face_exists(i32 x, i32 y, sight_side_t side)
{
    return tile_wall(x, y) && !tile_wall(x + SIDE_DX[side], y + SIDE_DY[side]);
}

static bool surface_lit(i32 x, i32 y, u32 surface)
{
    if (surface == 0)
        return tile_visible(x, y);

    return face_exists(x, y, (sight_side_t)(surface - 1)) && tile_visible(x, y);
}

static void write_surface(i32 x, i32 y, u32 surface, mat4 transform, sight_mask_instance_t *instance)
{
    *instance = (sight_mask_instance_t){ .transform = transform };

    if (surface_lit(x, y, surface))
        instance->flags |= 1u << SIGHT_LIT_BIT;
    instance->flags |= (u32)SIGHT_KNOWN_FULL << SIGHT_KNOWN_SHIFT;

    if (surface != 0)
    {
        sight_side_t side = (sight_side_t)(surface - 1);
        if (!tile_known(x + SIDE_DX[side], y + SIDE_DY[side]))
            instance->flags |= 1u << SIGHT_HIDDEN_BIT;
    }
}
