#include "sight.h"

#define SIGHT_HIDDEN_BIT              0

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
    bool known;
    bool lit;
    f32 lit_from;
    f32 lit_time;
    f32 reveal_time;
} sight_tile_t;

typedef struct
{
    const level_t *level;
    sight_tile_t *tiles;
    f32 wall_height;
    f32 clock;
} sight_t;

static sight_t g_sight;

static const i32 SIDE_DX[SIGHT_SIDE_COUNT] = { 0, 1, 0, -1 };
static const i32 SIDE_DY[SIGHT_SIDE_COUNT] = { -1, 0, 1, 0 };
static const f32 SIDE_YAW_DEG[SIGHT_SIDE_COUNT] = { 0.0f, 90.0f, 180.0f, -90.0f };

static bool tile_known(i32 x, i32 y);
static bool tile_wall(i32 x, i32 y);
static bool tile_visible(i32 x, i32 y);
static bool face_exists(i32 x, i32 y, sight_side_t side);
static f32  displayed_lit(const sight_tile_t *tile);
static f32  displayed_reveal(const sight_tile_t *tile);
static void write_surface(i32 x, i32 y, u32 surface, mat4 transform, sight_mask_instance_t *instance);

void Sight_Init(f32 wall_height)
{
    g_sight.wall_height = wall_height;
}

void Sight_Reset(const level_t *level, arena_t *run_arena)
{
    g_sight.level = level;
    g_sight.tiles = arena_push_array(run_arena, sight_tile_t, (u64)level->width * level->height);
    g_sight.clock = 0.0f;
}

void Sight_OnVisibilityChanged(void)
{
    sight_t *sight = &g_sight;
    const level_t *level = sight->level;

    for (i32 y = 0; y < level->height; y++)
    {
        for (i32 x = 0; x < level->width; x++)
        {
            sight_tile_t *tile = &sight->tiles[y * level->width + x];
            bool known = tile_known(x, y);
            bool lit = tile_visible(x, y);

            if (known && !tile->known)
            {
                tile->known = true;
                tile->reveal_time = sight->clock;
                tile->lit = lit;
                tile->lit_from = 0.0f;
                tile->lit_time = sight->clock + SIGHT_REVEAL_TIME - SIGHT_FADE_TIME;
            }
            else if (lit != tile->lit)
            {
                tile->lit_from = displayed_lit(tile);
                tile->lit = lit;
                tile->lit_time = sight->clock;
            }
        }
    }
}

void Sight_Update(f32 delta_time)
{
    g_sight.clock += delta_time;
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

static f32 displayed_lit(const sight_tile_t *tile)
{
    f32 progress = Clamp(0.0f, (g_sight.clock - tile->lit_time) / SIGHT_FADE_TIME, 1.0f);
    f32 target = tile->lit ? 1.0f : 0.0f;
    return tile->lit_from + (target - tile->lit_from) * progress;
}

static f32 displayed_reveal(const sight_tile_t *tile)
{
    return Clamp(0.0f, (g_sight.clock - tile->reveal_time) / SIGHT_REVEAL_TIME, 1.0f);
}

static void write_surface(i32 x, i32 y, u32 surface, mat4 transform, sight_mask_instance_t *instance)
{
    const sight_tile_t *tile = &g_sight.tiles[y * g_sight.level->width + x];

    *instance = (sight_mask_instance_t){
        .transform = transform,
        .lit = displayed_lit(tile),
        .reveal = displayed_reveal(tile),
    };

    if (surface != 0)
    {
        sight_side_t side = (sight_side_t)(surface - 1);
        if (!tile_known(x + SIDE_DX[side], y + SIDE_DY[side]))
            instance->flags |= 1u << SIGHT_HIDDEN_BIT;
    }
}
