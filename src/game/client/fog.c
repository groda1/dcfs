#include "fog.h"

#include <math.h>

#include "los.h"

#define FOG_WINDOW_SIZE             32
#define FOG_GRAPH_RADIUS            (LOS_RADIUS + 4)
#define FOG_GRAPH_SIZE              (2 * FOG_GRAPH_RADIUS + 1)
#define FOG_SURFACES_PER_TILE       5
#define FOG_NODES_PER_TILE          (FOG_SURFACES_PER_TILE * FOG_SURFACE_SAMPLES)
#define FOG_NODE_COUNT              (FOG_GRAPH_SIZE * FOG_GRAPH_SIZE * FOG_NODES_PER_TILE)
#define FOG_MAX_LINKS               32
#define FOG_SAMPLE_SPACING          0.5f
#define FOG_TEAR_REACH              (FOG_TEAR_DEPTH + FOG_EDGE_SOFTNESS)
#define FOG_KNOWN_LEAD              1.0f
#define FOG_FRONT_CLAMP             4.0f
#define FOG_NEVER                   1e30f

#define FOG_SELF_BIT                4
#define FOG_LEFT_BIT                3
#define FOG_RIGHT_BIT               5
#define FOG_REVEALING_SHIFT         9
#define FOG_KNOWN_SHIFT             18
#define FOG_HIDDEN_BIT              20

typedef enum
{
    FOG_BOX_NONE,
    FOG_BOX_LIT,
    FOG_BOX_PENDING,
} fog_box_t;

typedef enum
{
    FOG_KNOWN_DISSOLVE,
    FOG_KNOWN_FULL,
    FOG_KNOWN_FRONT,
} fog_known_t;

typedef enum
{
    FOG_SIDE_S,
    FOG_SIDE_E,
    FOG_SIDE_N,
    FOG_SIDE_W,
    FOG_SIDE_COUNT,
} fog_side_t;

typedef enum
{
    FOG_KIND_GROUND,
    FOG_KIND_TOP,
    FOG_KIND_SIDE,
} fog_kind_t;

typedef enum
{
    FOG_NODE_ABSENT,
    FOG_NODE_FIXED,
    FOG_NODE_OPEN,
} fog_node_state_t;

typedef struct
{
    i32 x;
    i32 y;
    bool valid;
    bool pending;
    bool known;
    bool revealing;
    f32 settle_time;
    f32 known_settle_time;
    u64 lit_nodes;
    f32 time[FOG_NODES_PER_TILE];
    f32 known_time[FOG_NODES_PER_TILE];
} fog_record_t;

typedef struct
{
    i32 x;
    i32 y;
    u32 slot;
    f32 cost;
} fog_link_t;

typedef struct
{
    level_t *level;
    u16 *previous_flags;
    f32 wall_height;
    f32 clock;

    fog_record_t *records;

    i32 base_x;
    i32 base_y;
    f32 *key;
    u8 *state;
    u8 *target;
    i32 *heap;
    i32 *heap_index;
    u32 heap_count;
} fog_t;

static fog_t g_fog;

static const i32 SIDE_DX[FOG_SIDE_COUNT] = { 0, 1, 0, -1 };
static const i32 SIDE_DY[FOG_SIDE_COUNT] = { -1, 0, 1, 0 };
static const i32 SIDE_UX[FOG_SIDE_COUNT] = { 1, 0, -1, 0 };
static const i32 SIDE_UY[FOG_SIDE_COUNT] = { 0, 1, 0, -1 };
static const i32 SIDE_START_X[FOG_SIDE_COUNT] = { 0, 2, 2, 0 };
static const i32 SIDE_START_Y[FOG_SIDE_COUNT] = { 0, 0, 2, 2 };
static const f32 SIDE_YAW_DEG[FOG_SIDE_COUNT] = { 0.0f, 90.0f, 180.0f, -90.0f };

static bool tile_known(i32 x, i32 y);
static bool tile_wall(i32 x, i32 y);
static bool tile_visible(i32 x, i32 y);
static bool face_exists(i32 x, i32 y, fog_side_t side);
static bool surface_exists(i32 x, i32 y, u32 surface);
static bool surface_target(i32 x, i32 y, u32 surface);
static fog_kind_t surface_kind(i32 x, i32 y, u32 surface);
static fog_side_t side_facing(i32 dx, i32 dy);
static i32  side_sample_at(i32 x, i32 y, fog_side_t side, i32 X2, i32 Y2);
static void node_position(i32 x, i32 y, u32 slot, i32 *X2, i32 *Y2, i32 *Z);
static u32  node_links(i32 x, i32 y, u32 slot, bool gain, fog_link_t *links);
static bool in_graph(i32 x, i32 y);
static i32  node_id(i32 x, i32 y, u32 slot);
static void node_tile(i32 id, i32 *x, i32 *y, u32 *slot);
static f32  fallback_time(i32 x, i32 y, u32 slot, bool gain, i32 origin_x, i32 origin_y);
static void prepare_light(void);
static void write_light(i32 origin_x, i32 origin_y);
static void prepare_known(void);
static void write_known(i32 origin_x, i32 origin_y);
static void run_graph(bool known_pass);
static void heap_place(u32 position, i32 node);
static void heap_update(i32 node);
static i32  heap_pop(void);
static fog_record_t *record_slot(i32 x, i32 y);
static const fog_record_t *window_record(i32 x, i32 y);
static bool tile_pending(i32 x, i32 y);
static bool tile_revealing(i32 x, i32 y);
static fog_box_t surface_box(i32 x, i32 y, u32 surface);
static bool node_lit(const fog_record_t *record, u32 slot);
static f32  front_distance(const fog_record_t *record, u32 slot);
static f32  known_distance(const fog_record_t *record, u32 slot);
static bool slot_at(i32 x, i32 y, i32 X2, i32 Y2, i32 Z, bool any_height, u32 *slot);
static void write_surface(i32 x, i32 y, u32 surface, mat4 transform, fog_mask_instance_t *instance);
static void write_tile_neighbours(i32 x, i32 y, fog_mask_instance_t *instance);
static void write_side_neighbours(i32 x, i32 y, fog_side_t side, fog_mask_instance_t *instance);
static bool side_neighbour(i32 x, i32 y, fog_side_t side, i32 dir, i32 *nx, i32 *ny, fog_side_t *nside);

void Fog_Init(arena_t *arena, f32 wall_height)
{
    fog_t *fog = &g_fog;

    fog->wall_height = wall_height;
    fog->records = arena_push_array(arena, fog_record_t, FOG_WINDOW_SIZE * FOG_WINDOW_SIZE);
    fog->key = arena_push_array(arena, f32, FOG_NODE_COUNT);
    fog->state = arena_push_array(arena, u8, FOG_NODE_COUNT);
    fog->target = arena_push_array(arena, u8, FOG_NODE_COUNT);
    fog->heap = arena_push_array(arena, i32, FOG_NODE_COUNT);
    fog->heap_index = arena_push_array(arena, i32, FOG_NODE_COUNT);
}

void Fog_Reset(level_t *level, arena_t *run_arena)
{
    fog_t *fog = &g_fog;

    fog->level = level;
    fog->previous_flags = arena_push_array(run_arena, u16, (u64)level->width * level->height);
    fog->clock = 0.0f;

    for (u32 i = 0; i < FOG_WINDOW_SIZE * FOG_WINDOW_SIZE; i++)
        fog->records[i].valid = false;
}

void Fog_Snapshot(void)
{
    const level_t *level = g_fog.level;
    u64 tile_count = (u64)level->width * level->height;

    for (u64 i = 0; i < tile_count; i++)
        g_fog.previous_flags[i] = level->tiles[i].flags;
}

void Fog_OnVisibilityChanged(i32 origin_x, i32 origin_y)
{
    fog_t *fog = &g_fog;

    fog->base_x = origin_x - FOG_GRAPH_RADIUS;
    fog->base_y = origin_y - FOG_GRAPH_RADIUS;

    prepare_light();
    run_graph(false);
    write_light(origin_x, origin_y);

    prepare_known();
    run_graph(true);
    write_known(origin_x, origin_y);
}

void Fog_Update(f32 delta_time)
{
    fog_t *fog = &g_fog;
    bool any_pending = false;

    fog->clock += delta_time;

    for (u32 i = 0; i < FOG_WINDOW_SIZE * FOG_WINDOW_SIZE; i++)
    {
        fog_record_t *record = &fog->records[i];
        if (!record->valid)
            continue;

        if (record->pending && fog->clock >= record->settle_time)
            record->pending = false;

        if (record->revealing && fog->clock >= record->known_settle_time)
        {
            record->revealing = false;
            record->known = true;
        }

        any_pending = any_pending || record->pending || record->revealing;
    }

    if (!any_pending)
        fog->clock = 0.0f;
}

u32 Fog_WriteInstances(i32 x, i32 y, vec3 center, fog_mask_instance_t *instances)
{
    f32 height = g_fog.wall_height;
    bool wall = tile_wall(x, y);
    u32 count = 0;

    vec3 top_center = center;
    if (wall)
        top_center.Y = height;

    mat4 flat = HMM_Rotate_RH(HMM_AngleDeg(-90.0f), V3(1.0f, 0.0f, 0.0f));
    write_surface(x, y, 0, HMM_MulM4(HMM_Translate(top_center), flat), &instances[count++]);

    if (!wall)
        return count;

    for (i32 side = 0; side < FOG_SIDE_COUNT; side++)
    {
        if (!face_exists(x, y, (fog_side_t)side))
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
    const level_t *level = g_fog.level;
    if (!Level_InBounds(level, x, y))
        return false;

    const tile_t *tile = Level_GetTile(level, x, y);
    return (tile->flags & FLAG_REVEALED) && tile->type != TILE_EMPTY;
}

static bool tile_wall(i32 x, i32 y)
{
    return tile_known(x, y) && Level_GetTile(g_fog.level, x, y)->type == TILE_WALL;
}

static bool tile_visible(i32 x, i32 y)
{
    return tile_known(x, y) && (Level_GetTile(g_fog.level, x, y)->flags & FLAG_VISIBLE);
}

static bool face_exists(i32 x, i32 y, fog_side_t side)
{
    return tile_wall(x, y) && !tile_wall(x + SIDE_DX[side], y + SIDE_DY[side]);
}

static bool surface_exists(i32 x, i32 y, u32 surface)
{
    if (surface == 0)
        return tile_known(x, y);

    return face_exists(x, y, (fog_side_t)(surface - 1));
}

static bool surface_target(i32 x, i32 y, u32 surface)
{
    if (!tile_visible(x, y))
        return false;
    if (surface == 0)
        return true;

    fog_side_t side = (fog_side_t)(surface - 1);
    return tile_visible(x + SIDE_DX[side], y + SIDE_DY[side]);
}

static fog_kind_t surface_kind(i32 x, i32 y, u32 surface)
{
    if (surface != 0)
        return FOG_KIND_SIDE;

    return tile_wall(x, y) ? FOG_KIND_TOP : FOG_KIND_GROUND;
}

static fog_side_t side_facing(i32 dx, i32 dy)
{
    for (i32 side = 0; side < FOG_SIDE_COUNT; side++)
    {
        if (SIDE_DX[side] == dx && SIDE_DY[side] == dy)
            return (fog_side_t)side;
    }

    return FOG_SIDE_S;
}

static i32 side_sample_at(i32 x, i32 y, fog_side_t side, i32 X2, i32 Y2)
{
    i32 ox = X2 - 2 * x - SIDE_START_X[side];
    i32 oy = Y2 - 2 * y - SIDE_START_Y[side];

    i32 i = SIDE_UX[side] != 0 ? ox * SIDE_UX[side] : oy * SIDE_UY[side];
    i32 across = SIDE_UX[side] != 0 ? oy : ox;
    if (across != 0 || i < 0 || i > 2)
        return -1;

    return i;
}

static void node_position(i32 x, i32 y, u32 slot, i32 *X2, i32 *Y2, i32 *Z)
{
    u32 surface = slot / FOG_SURFACE_SAMPLES;
    i32 i = (i32)(slot % 3);
    i32 j = (i32)((slot % FOG_SURFACE_SAMPLES) / 3);

    if (surface == 0)
    {
        *X2 = 2 * x + i;
        *Y2 = 2 * y + j;
        *Z = tile_wall(x, y) ? 2 : 0;
        return;
    }

    fog_side_t side = (fog_side_t)(surface - 1);
    *X2 = 2 * x + SIDE_START_X[side] + i * SIDE_UX[side];
    *Y2 = 2 * y + SIDE_START_Y[side] + i * SIDE_UY[side];
    *Z = j;
}

static u32 node_links(i32 x, i32 y, u32 slot, bool gain, fog_link_t *links)
{
    u32 count = 0;
    u32 surface = slot / FOG_SURFACE_SAMPLES;
    i32 i = (i32)(slot % 3);
    i32 j = (i32)((slot % FOG_SURFACE_SAMPLES) / 3);
    f32 step_v = surface == 0 ? FOG_SAMPLE_SPACING : 0.5f * g_fog.wall_height;
    fog_kind_t kind = surface_kind(x, y, surface);

    for (i32 dj = -1; dj <= 1; dj++)
    {
        for (i32 di = -1; di <= 1; di++)
        {
            i32 ni = i + di;
            i32 nj = j + dj;
            if ((di == 0 && dj == 0) || ni < 0 || ni > 2 || nj < 0 || nj > 2)
                continue;

            f32 du = FOG_SAMPLE_SPACING * (f32)di;
            f32 dv = step_v * (f32)dj;
            links[count++] = (fog_link_t){
                x, y, surface * FOG_SURFACE_SAMPLES + (u32)(nj * 3 + ni), sqrtf(du * du + dv * dv),
            };
        }
    }

    i32 X2, Y2, Z;
    node_position(x, y, slot, &X2, &Y2, &Z);

    for (i32 ty = (Y2 >> 1) - 1; ty <= (Y2 >> 1); ty++)
    {
        for (i32 tx = (X2 >> 1) - 1; tx <= (X2 >> 1); tx++)
        {
            i32 ti = X2 - 2 * tx;
            i32 tj = Y2 - 2 * ty;
            if (ti < 0 || ti > 2 || tj < 0 || tj > 2 || !tile_known(tx, ty))
                continue;

            for (u32 other_surface = 0; other_surface < FOG_SURFACES_PER_TILE; other_surface++)
            {
                if (!surface_exists(tx, ty, other_surface))
                    continue;

                u32 other;
                if (other_surface == 0)
                {
                    if (Z != (tile_wall(tx, ty) ? 2 : 0))
                        continue;
                    other = (u32)(tj * 3 + ti);
                }
                else
                {
                    i32 k = side_sample_at(tx, ty, (fog_side_t)(other_surface - 1), X2, Y2);
                    if (k < 0)
                        continue;
                    other = other_surface * FOG_SURFACE_SAMPLES + (u32)(Z * 3 + k);
                }

                if (tx == x && ty == y && other == slot)
                    continue;

                fog_kind_t other_kind = surface_kind(tx, ty, other_surface);
                f32 cost = (other_kind != kind && gain) ? FOG_TEAR_REACH : 0.0f;
                links[count++] = (fog_link_t){ tx, ty, other, cost };
            }
        }
    }

    Assert(count <= FOG_MAX_LINKS);
    return count;
}

static bool in_graph(i32 x, i32 y)
{
    return x >= g_fog.base_x && x < g_fog.base_x + FOG_GRAPH_SIZE
        && y >= g_fog.base_y && y < g_fog.base_y + FOG_GRAPH_SIZE;
}

static i32 node_id(i32 x, i32 y, u32 slot)
{
    i32 tile = (y - g_fog.base_y) * FOG_GRAPH_SIZE + (x - g_fog.base_x);
    return tile * FOG_NODES_PER_TILE + (i32)slot;
}

static void node_tile(i32 id, i32 *x, i32 *y, u32 *slot)
{
    i32 tile = id / FOG_NODES_PER_TILE;
    *slot = (u32)(id % FOG_NODES_PER_TILE);
    *x = g_fog.base_x + tile % FOG_GRAPH_SIZE;
    *y = g_fog.base_y + tile / FOG_GRAPH_SIZE;
}

static f32 fallback_time(i32 x, i32 y, u32 slot, bool gain, i32 origin_x, i32 origin_y)
{
    i32 X2, Y2, Z;
    node_position(x, y, slot, &X2, &Y2, &Z);

    f32 dx = 0.5f * (f32)X2 - ((f32)origin_x + 0.5f);
    f32 dy = 0.5f * (f32)Y2 - ((f32)origin_y + 0.5f);
    f32 distance = sqrtf(dx * dx + dy * dy);

    if (gain)
        return g_fog.clock + distance / FOG_SPEED;

    return g_fog.clock + Max(0.0f, (f32)LOS_RADIUS + 1.0f - distance) / FOG_SPEED;
}

static void prepare_light(void)
{
    fog_t *fog = &g_fog;

    fog->heap_count = 0;

    for (i32 y = fog->base_y; y < fog->base_y + FOG_GRAPH_SIZE; y++)
    {
        for (i32 x = fog->base_x; x < fog->base_x + FOG_GRAPH_SIZE; x++)
        {
            fog_record_t *record = record_slot(x, y);
            if (!record->valid || record->x != x || record->y != y)
            {
                *record = (fog_record_t){
                    .x = x,
                    .y = y,
                    .valid = true,
                    .known = tile_known(x, y)
                          && (fog->previous_flags[y * fog->level->width + x] & FLAG_REVEALED),
                };
            }

            for (u32 slot = 0; slot < FOG_NODES_PER_TILE; slot++)
            {
                i32 id = node_id(x, y, slot);
                fog->heap_index[id] = -1;

                if (!surface_exists(x, y, slot / FOG_SURFACE_SAMPLES))
                {
                    fog->state[id] = FOG_NODE_ABSENT;
                    continue;
                }

                bool target = surface_target(x, y, slot / FOG_SURFACE_SAMPLES);
                bool lit = node_lit(record, slot);
                f32 time = record->pending ? record->time[slot] : -FOG_NEVER;
                bool current = fog->clock >= time ? lit : !lit;
                fog->target[id] = target;

                if (current == target)
                {
                    if (lit != target)
                        time = 2.0f * fog->clock - time;
                    record->time[slot] = time;
                    fog->state[id] = FOG_NODE_FIXED;
                    fog->key[id] = fog->clock;
                }
                else
                {
                    fog->state[id] = FOG_NODE_OPEN;
                    fog->key[id] = lit == target ? time : FOG_NEVER;
                }

                if (fog->key[id] < FOG_NEVER)
                    heap_update(id);
            }
        }
    }
}

static void write_light(i32 origin_x, i32 origin_y)
{
    fog_t *fog = &g_fog;

    for (i32 y = fog->base_y; y < fog->base_y + FOG_GRAPH_SIZE; y++)
    {
        for (i32 x = fog->base_x; x < fog->base_x + FOG_GRAPH_SIZE; x++)
        {
            fog_record_t *record = record_slot(x, y);
            f32 settle = -FOG_NEVER;

            record->lit_nodes = 0;
            for (u32 slot = 0; slot < FOG_NODES_PER_TILE; slot++)
            {
                i32 id = node_id(x, y, slot);
                bool target = surface_target(x, y, slot / FOG_SURFACE_SAMPLES);
                if (target)
                    record->lit_nodes |= 1ull << slot;

                if (fog->state[id] == FOG_NODE_ABSENT)
                {
                    record->time[slot] = -FOG_NEVER;
                    continue;
                }

                if (fog->state[id] == FOG_NODE_OPEN)
                {
                    f32 time = fog->key[id];
                    if (time >= FOG_NEVER)
                        time = fallback_time(x, y, slot, target, origin_x, origin_y);
                    record->time[slot] = time;
                }

                settle = Max(settle, record->time[slot]);
            }

            record->settle_time = settle + FOG_TEAR_REACH / FOG_SPEED;
            record->pending = record->settle_time > fog->clock;
        }
    }
}

static void prepare_known(void)
{
    fog_t *fog = &g_fog;

    fog->heap_count = 0;

    for (i32 y = fog->base_y; y < fog->base_y + FOG_GRAPH_SIZE; y++)
    {
        for (i32 x = fog->base_x; x < fog->base_x + FOG_GRAPH_SIZE; x++)
        {
            const fog_record_t *record = record_slot(x, y);

            for (u32 slot = 0; slot < FOG_NODES_PER_TILE; slot++)
            {
                i32 id = node_id(x, y, slot);
                fog->heap_index[id] = -1;

                if (fog->state[id] == FOG_NODE_ABSENT)
                    continue;

                f32 key = !record->known && record->revealing ? record->known_time[slot] : FOG_NEVER;
                if (fog->target[id])
                {
                    f32 lit_time = record->pending ? record->time[slot] : fog->clock;
                    key = Min(key, lit_time - FOG_KNOWN_LEAD / FOG_SPEED);
                }

                if (record->known && !fog->target[id])
                    fog->state[id] = FOG_NODE_ABSENT;
                else
                    fog->state[id] = record->known ? FOG_NODE_FIXED : FOG_NODE_OPEN;
                fog->key[id] = key;

                if (fog->key[id] < FOG_NEVER)
                    heap_update(id);
            }
        }
    }
}

static void write_known(i32 origin_x, i32 origin_y)
{
    fog_t *fog = &g_fog;

    for (i32 y = fog->base_y; y < fog->base_y + FOG_GRAPH_SIZE; y++)
    {
        for (i32 x = fog->base_x; x < fog->base_x + FOG_GRAPH_SIZE; x++)
        {
            fog_record_t *record = record_slot(x, y);
            if (record->known)
                continue;

            if (!tile_known(x, y))
            {
                record->revealing = false;
                continue;
            }

            f32 settle = -FOG_NEVER;

            for (u32 slot = 0; slot < FOG_NODES_PER_TILE; slot++)
            {
                i32 id = node_id(x, y, slot);
                if (fog->state[id] == FOG_NODE_ABSENT)
                {
                    record->known_time[slot] = FOG_NEVER;
                    continue;
                }

                f32 time = fog->key[id];
                if (time >= FOG_NEVER)
                    time = fallback_time(x, y, slot, true, origin_x, origin_y);

                record->known_time[slot] = time;
                settle = Max(settle, time);
            }

            record->known_settle_time = settle + FOG_TEAR_REACH / FOG_SPEED;
            record->revealing = record->known_settle_time > fog->clock;
            record->known = !record->revealing;
        }
    }
}

static void run_graph(bool known_pass)
{
    fog_t *fog = &g_fog;
    fog_link_t links[FOG_MAX_LINKS];

    while (fog->heap_count > 0)
    {
        i32 id = heap_pop();
        i32 x, y;
        u32 slot;
        node_tile(id, &x, &y, &slot);

        bool gain = !known_pass && fog->target[id];
        f32 start = known_pass ? Max(fog->key[id], fog->clock) : fog->key[id];
        u32 link_count = node_links(x, y, slot, gain, links);

        for (u32 i = 0; i < link_count; i++)
        {
            if (!in_graph(links[i].x, links[i].y))
                continue;

            i32 other = node_id(links[i].x, links[i].y, links[i].slot);
            if (fog->state[other] != FOG_NODE_OPEN)
                continue;
            if (!known_pass && fog->target[other] != fog->target[id])
                continue;

            f32 arrival = start + links[i].cost / FOG_SPEED;
            if (arrival < fog->key[other])
            {
                fog->key[other] = arrival;
                heap_update(other);
            }
        }
    }
}

static void heap_place(u32 position, i32 node)
{
    g_fog.heap[position] = node;
    g_fog.heap_index[node] = (i32)position;
}

static void heap_update(i32 node)
{
    fog_t *fog = &g_fog;

    u32 position = fog->heap_index[node] < 0 ? fog->heap_count++ : (u32)fog->heap_index[node];
    while (position > 0)
    {
        u32 parent = (position - 1) / 2;
        i32 parent_node = fog->heap[parent];
        if (fog->key[parent_node] <= fog->key[node])
            break;

        heap_place(position, parent_node);
        position = parent;
    }

    heap_place(position, node);
}

static i32 heap_pop(void)
{
    fog_t *fog = &g_fog;

    i32 top = fog->heap[0];
    fog->heap_index[top] = -1;

    i32 last = fog->heap[--fog->heap_count];
    if (fog->heap_count == 0)
        return top;

    u32 position = 0;
    for (;;)
    {
        u32 child = position * 2 + 1;
        if (child >= fog->heap_count)
            break;
        if (child + 1 < fog->heap_count && fog->key[fog->heap[child + 1]] < fog->key[fog->heap[child]])
            child++;
        if (fog->key[fog->heap[child]] >= fog->key[last])
            break;

        heap_place(position, fog->heap[child]);
        position = child;
    }

    heap_place(position, last);
    return top;
}

static fog_record_t *record_slot(i32 x, i32 y)
{
    u32 wx = (u32)x & (FOG_WINDOW_SIZE - 1);
    u32 wy = (u32)y & (FOG_WINDOW_SIZE - 1);
    return &g_fog.records[wy * FOG_WINDOW_SIZE + wx];
}

static const fog_record_t *window_record(i32 x, i32 y)
{
    const fog_record_t *record = record_slot(x, y);
    if (!record->valid || record->x != x || record->y != y)
        return 0;

    return record;
}

static bool tile_pending(i32 x, i32 y)
{
    const fog_record_t *record = window_record(x, y);
    return record && record->pending;
}

static bool tile_revealing(i32 x, i32 y)
{
    const fog_record_t *record = window_record(x, y);
    return tile_known(x, y) && record && record->revealing;
}

static fog_box_t surface_box(i32 x, i32 y, u32 surface)
{
    if (!surface_exists(x, y, surface))
        return FOG_BOX_NONE;
    if (tile_pending(x, y))
        return FOG_BOX_PENDING;

    return surface_target(x, y, surface) ? FOG_BOX_LIT : FOG_BOX_NONE;
}

static bool node_lit(const fog_record_t *record, u32 slot)
{
    return (record->lit_nodes >> slot) & 1u;
}

static f32 front_distance(const fog_record_t *record, u32 slot)
{
    f32 remaining = FOG_SPEED * (record->time[slot] - g_fog.clock);
    f32 distance = node_lit(record, slot) ? remaining : -remaining;
    return Clamp(-FOG_FRONT_CLAMP, distance, FOG_FRONT_CLAMP);
}

static f32 known_distance(const fog_record_t *record, u32 slot)
{
    f32 distance = FOG_SPEED * (record->known_time[slot] - g_fog.clock);
    return Clamp(-FOG_FRONT_CLAMP, distance, FOG_FRONT_CLAMP);
}

static bool slot_at(i32 x, i32 y, i32 X2, i32 Y2, i32 Z, bool any_height, u32 *slot)
{
    i32 i = X2 - 2 * x;
    i32 j = Y2 - 2 * y;
    if (i < 0 || i > 2 || j < 0 || j > 2 || !tile_known(x, y))
        return false;

    if (Z == (tile_wall(x, y) ? 2 : 0))
    {
        *slot = (u32)(j * 3 + i);
        return true;
    }

    for (u32 surface = 1; surface < FOG_SURFACES_PER_TILE; surface++)
    {
        if (!surface_exists(x, y, surface))
            continue;

        i32 k = side_sample_at(x, y, (fog_side_t)(surface - 1), X2, Y2);
        if (k >= 0)
        {
            *slot = surface * FOG_SURFACE_SAMPLES + (u32)(Z * 3 + k);
            return true;
        }
    }

    if (!any_height)
        return false;

    *slot = (u32)(j * 3 + i);
    return true;
}

static void write_surface(i32 x, i32 y, u32 surface, mat4 transform, fog_mask_instance_t *instance)
{
    *instance = (fog_mask_instance_t){ .transform = transform };

    const fog_record_t *record = window_record(x, y);
    bool pending = record && record->pending;
    bool revealing = record && record->revealing;

    instance->boxes |= (u32)surface_box(x, y, surface) << (2 * FOG_SELF_BIT);
    instance->flags |= (u32)(revealing ? FOG_KNOWN_FRONT : FOG_KNOWN_FULL) << FOG_KNOWN_SHIFT;

    for (u32 sample = 0; sample < FOG_SURFACE_SAMPLES; sample++)
    {
        u32 slot = surface * FOG_SURFACE_SAMPLES + sample;
        if (pending)
            instance->surface[sample] = front_distance(record, slot);
        if (revealing)
            instance->known_surface[sample] = known_distance(record, slot);
    }

    if (surface == 0)
        write_tile_neighbours(x, y, instance);
    else
        write_side_neighbours(x, y, (fog_side_t)(surface - 1), instance);
}

static void write_tile_neighbours(i32 x, i32 y, fog_mask_instance_t *instance)
{
    static const i32 EDGE_TILE[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    static const i32 EDGE_POINT[4][4] = {
        { 0, 0, 1, 0 }, { 2, 0, 0, 1 }, { 0, 2, 1, 0 }, { 0, 0, 0, 1 },
    };
    static const i32 CORNER[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };

    bool wall = tile_wall(x, y);
    i32 Z = wall ? 2 : 0;

    for (i32 n = 0; n < 8; n++)
    {
        i32 dx = n < 4 ? EDGE_TILE[n][0] : CORNER[n - 4][0];
        i32 dy = n < 4 ? EDGE_TILE[n][1] : CORNER[n - 4][1];
        i32 nx = x + dx;
        i32 ny = y + dy;
        u32 bit = (u32)((dy + 1) * 3 + (dx + 1));

        if (!tile_known(nx, ny))
        {
            instance->flags |= 1u << bit;
            continue;
        }

        bool same_kind = tile_wall(nx, ny) == wall;
        fog_box_t box = same_kind ? surface_box(nx, ny, 0) : FOG_BOX_NONE;
        bool revealing = tile_revealing(nx, ny);
        const fog_record_t *record = window_record(nx, ny);

        instance->boxes |= (u32)box << (2 * bit);
        if (revealing)
            instance->flags |= 1u << (FOG_REVEALING_SHIFT + bit);

        i32 points = n < 4 ? 3 : 1;
        for (i32 k = 0; k < points; k++)
        {
            i32 X2, Y2, edge;
            if (n < 4)
            {
                X2 = 2 * x + EDGE_POINT[n][0] + k * EDGE_POINT[n][2];
                Y2 = 2 * y + EDGE_POINT[n][1] + k * EDGE_POINT[n][3];
                edge = n * 3 + k;
            }
            else
            {
                X2 = 2 * x + (dx > 0 ? 2 : 0);
                Y2 = 2 * y + (dy > 0 ? 2 : 0);
                edge = 12 + (n - 4);
            }

            u32 slot;
            if (box == FOG_BOX_PENDING && slot_at(nx, ny, X2, Y2, Z, false, &slot))
                instance->edge[edge] = front_distance(record, slot);
            if (revealing && slot_at(nx, ny, X2, Y2, Z, true, &slot))
                instance->known_edge[edge] = known_distance(record, slot);
        }
    }
}

static void write_side_neighbours(i32 x, i32 y, fog_side_t side, fog_mask_instance_t *instance)
{
    if (!tile_known(x + SIDE_DX[side], y + SIDE_DY[side]))
        instance->flags |= 1u << FOG_HIDDEN_BIT;

    for (i32 dir = -1; dir <= 1; dir += 2)
    {
        u32 bit = dir < 0 ? FOG_LEFT_BIT : FOG_RIGHT_BIT;
        i32 first_edge = dir < 0 ? 9 : 3;

        i32 nx, ny;
        fog_side_t nside;
        if (!side_neighbour(x, y, side, dir, &nx, &ny, &nside))
        {
            instance->flags |= 1u << bit;
            continue;
        }

        u32 nsurface = 1 + (u32)nside;
        fog_box_t box = surface_box(nx, ny, nsurface);
        bool revealing = tile_revealing(nx, ny);
        const fog_record_t *record = window_record(nx, ny);

        instance->boxes |= (u32)box << (2 * bit);
        if (revealing)
            instance->flags |= 1u << (FOG_REVEALING_SHIFT + bit);

        i32 X2 = 2 * x + SIDE_START_X[side] + (dir < 0 ? 0 : 2) * SIDE_UX[side];
        i32 Y2 = 2 * y + SIDE_START_Y[side] + (dir < 0 ? 0 : 2) * SIDE_UY[side];
        i32 k = side_sample_at(nx, ny, nside, X2, Y2);
        if (k < 0)
            continue;

        for (i32 j = 0; j < 3; j++)
        {
            u32 slot = nsurface * FOG_SURFACE_SAMPLES + (u32)(j * 3 + k);
            if (box == FOG_BOX_PENDING)
                instance->edge[first_edge + j] = front_distance(record, slot);
            if (revealing)
                instance->known_edge[first_edge + j] = known_distance(record, slot);
        }
    }
}

static bool side_neighbour(i32 x, i32 y, fog_side_t side, i32 dir, i32 *nx, i32 *ny, fog_side_t *nside)
{
    i32 lx = x + dir * SIDE_UX[side];
    i32 ly = y + dir * SIDE_UY[side];

    if (!tile_known(lx, ly))
        return false;

    if (!tile_wall(lx, ly))
    {
        *nx = x;
        *ny = y;
        *nside = side_facing(dir * SIDE_UX[side], dir * SIDE_UY[side]);
        return true;
    }

    if (face_exists(lx, ly, side))
    {
        *nx = lx;
        *ny = ly;
        *nside = side;
        return true;
    }

    *nx = lx + SIDE_DX[side];
    *ny = ly + SIDE_DY[side];
    *nside = side_facing(-dir * SIDE_UX[side], -dir * SIDE_UY[side]);
    return face_exists(*nx, *ny, *nside);
}
