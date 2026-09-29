#include "los.h"
#include "log.h"
#include "debug_category.h"
#include "os_time.h"
#include "rules.h"

#define LOS_RAYS_PER_TILE   8
#define LOS_DIAMOND_RADIUS  0.5f
#define LOS_DIAMOND_MARGIN  1e-4f
#define LOS_START_REACH     0.48f
#define LOS_NO_NODE         0xFFFFFFFFu

typedef struct
{
    i8 dx;
    i8 dy;
    u32 first_child;
    u32 next_sibling;
} los_node_t;

typedef struct
{
    i32 radius;
    u32 ray_count;
    u32 node_count;
    u32 node_capacity;
    los_node_t *nodes;
} los_tree_t;

static const f32 LOS_STARTS[][2] = {
    { 0.0f, 0.0f },
    { 0.0f, -LOS_START_REACH },
    { LOS_START_REACH, 0.0f },
    { 0.0f, LOS_START_REACH },
    { -LOS_START_REACH, 0.0f },
};

static void build_tree(los_tree_t *tree, arena_t *arena, i32 radius);
static void add_ray(los_tree_t *tree, f32 start_x, f32 start_y, f32 target_x, f32 target_y);
static u32  child_node(los_tree_t *tree, u32 parent, i8 dx, i8 dy);
static void flatten(const los_tree_t *tree, u32 node, los_t *los);
static f32  first_crossing(f32 start, i32 cell, f32 delta);
static bool passes_diamond(f32 start_x, f32 start_y, f32 delta_x, f32 delta_y,
                           f32 t0, f32 t1, i32 x, i32 y);
static f32  diamond_distance(f32 start_x, f32 start_y, f32 delta_x, f32 delta_y,
                             f32 t, f32 center_x, f32 center_y);
static f32  absolute(f32 value);

void LoS_Init(los_t *los, arena_t *arena, i32 radius)
{
    Assert(radius >= 0 && radius <= 127);

    u64 start_ns = OS_TimeNowNs();
    los_tree_t tree;
    scratch_t scratch = Scratch_Begin(arena);
    build_tree(&tree, arena, radius);
    u32 cell_count = tree.node_count - 1;
    Scratch_End(scratch);

    los->radius = radius;
    los->cell_count = 0;
    los->cells = arena_push_array(arena, los_cell_t, cell_count);

    scratch = Scratch_Begin(arena);
    build_tree(&tree, arena, radius);
    for (u32 child = tree.nodes[0].first_child; child != LOS_NO_NODE; child = tree.nodes[child].next_sibling)
        flatten(&tree, child, los);
    Scratch_End(scratch);

    Assert(los->cell_count == cell_count);

    DEBUG(LOS, "radius %d, %u rays, %u cells, %u bytes kept, %u bytes scratch, built in %.2f ms",
        radius, tree.ray_count, los->cell_count,
        (u32)(los->cell_count * sizeof(los_cell_t)),
        (u32)(tree.node_capacity * sizeof(los_node_t)),
        (f64)(OS_TimeNowNs() - start_ns) / 1e6);
}

void LoS_Compute(const los_t *los, level_t *level, i32 origin_x, i32 origin_y)
{
    // TODO: optimize
    u64 tile_count = (u64)level->width * level->height;
    for (u64 i = 0; i < tile_count; i++)
        level->tiles[i].flags &= (u16)~FLAG_VISIBLE;

    if (!Level_InBounds(level, origin_x, origin_y))
        return;

    Level_GetTile(level, origin_x, origin_y)->flags |= FLAG_VISIBLE;

    u32 i = 0;
    while (i < los->cell_count)
    {
        const los_cell_t *cell = &los->cells[i];
        i32 x = origin_x + cell->dx;
        i32 y = origin_y + cell->dy;

        if (!Level_InBounds(level, x, y))
        {
            i = cell->end;
            continue;
        }

        Level_GetTile(level, x, y)->flags |= FLAG_VISIBLE;

        i = Rules_BlocksSight(level, x, y) ? cell->end : i + 1;
    }
}

bool LoS_TouchesVisibleOpenTile(const level_t *level, i32 x, i32 y)
{
    for (i32 ny = y - 1; ny <= y + 1; ny++)
    {
        for (i32 nx = x - 1; nx <= x + 1; nx++)
        {
            if (!Level_InBounds(level, nx, ny) || Rules_BlocksSight(level, nx, ny))
                continue;

            if (Level_GetTile(level, nx, ny)->flags & FLAG_VISIBLE)
                return true;
        }
    }

    return false;
}

static void build_tree(los_tree_t *tree, arena_t *arena, i32 radius)
{
    u32 ray_count = (u32)(2 * radius + 1) * LOS_RAYS_PER_TILE;
    u32 max_ray_cells = (u32)(2 * radius + 2);

    tree->radius = radius;
    tree->ray_count = 0;
    tree->node_count = 1;
    tree->node_capacity = 1 + (u32)ArrayCount(LOS_STARTS) * 4 * ray_count * max_ray_cells;
    tree->nodes = arena_push_array_no_zero(arena, los_node_t, tree->node_capacity);
    tree->nodes[0] = (los_node_t){ .first_child = LOS_NO_NODE, .next_sibling = LOS_NO_NODE };

    f32 left = (f32)-radius;
    f32 right = (f32)(radius + 1);
    f32 bottom = (f32)-radius;
    f32 top = (f32)(radius + 1);

    for (u32 s = 0; s < ArrayCount(LOS_STARTS); s++)
    {
        f32 start_x = 0.5f + LOS_STARTS[s][0];
        f32 start_y = 0.5f + LOS_STARTS[s][1];

        for (u32 i = 0; i < ray_count; i++)
        {
            f32 along = ((f32)i + 0.5f) / LOS_RAYS_PER_TILE;
            add_ray(tree, start_x, start_y, left + along, bottom);
            add_ray(tree, start_x, start_y, left + along, top);
            add_ray(tree, start_x, start_y, left, bottom + along);
            add_ray(tree, start_x, start_y, right, bottom + along);
        }
    }
}

static void add_ray(los_tree_t *tree, f32 start_x, f32 start_y, f32 target_x, f32 target_y)
{
    f32 delta_x = target_x - start_x;
    f32 delta_y = target_y - start_y;
    if (delta_x == 0.0f && delta_y == 0.0f)
        return;

    tree->ray_count++;

    i32 radius = tree->radius;
    i32 x = 0;
    i32 y = 0;
    i32 step_x = delta_x >= 0.0f ? 1 : -1;
    i32 step_y = delta_y >= 0.0f ? 1 : -1;
    f32 advance_x = (f32)step_x / delta_x;
    f32 advance_y = (f32)step_y / delta_y;
    f32 next_x = first_crossing(start_x, x, delta_x);
    f32 next_y = first_crossing(start_y, y, delta_y);
    u32 node = 0;

    for (;;)
    {
        f32 entry = Min(next_x, next_y);
        if (next_x < next_y)
        {
            x += step_x;
            next_x += advance_x;
        }
        else
        {
            y += step_y;
            next_y += advance_y;
        }
        f32 exit = Min(next_x, next_y);

        if (x < -radius || x > radius || y < -radius || y > radius)
            return;

        if (passes_diamond(start_x, start_y, delta_x, delta_y, entry, exit, x, y))
            node = child_node(tree, node, (i8)x, (i8)y);
    }
}

static u32 child_node(los_tree_t *tree, u32 parent, i8 dx, i8 dy)
{
    u32 *link = &tree->nodes[parent].first_child;
    while (*link != LOS_NO_NODE)
    {
        los_node_t *node = &tree->nodes[*link];
        if (node->dx == dx && node->dy == dy)
            return *link;
        link = &node->next_sibling;
    }

    Assert(tree->node_count < tree->node_capacity);
    u32 index = tree->node_count++;
    tree->nodes[index] = (los_node_t){
        .dx = dx,
        .dy = dy,
        .first_child = LOS_NO_NODE,
        .next_sibling = LOS_NO_NODE,
    };
    *link = index;
    return index;
}

static void flatten(const los_tree_t *tree, u32 node, los_t *los)
{
    u32 index = los->cell_count++;
    los->cells[index].dx = tree->nodes[node].dx;
    los->cells[index].dy = tree->nodes[node].dy;

    for (u32 child = tree->nodes[node].first_child; child != LOS_NO_NODE; child = tree->nodes[child].next_sibling)
        flatten(tree, child, los);

    los->cells[index].end = los->cell_count;
}

static f32 first_crossing(f32 start, i32 cell, f32 delta)
{
    if (delta >= 0.0f)
        return ((f32)(cell + 1) - start) / delta;

    return ((f32)cell - start) / delta;
}

static bool passes_diamond(f32 start_x, f32 start_y, f32 delta_x, f32 delta_y,
                           f32 t0, f32 t1, i32 x, i32 y)
{
    f32 center_x = (f32)x + 0.5f;
    f32 center_y = (f32)y + 0.5f;

    f32 closest = Min(diamond_distance(start_x, start_y, delta_x, delta_y, t0, center_x, center_y),
                      diamond_distance(start_x, start_y, delta_x, delta_y, t1, center_x, center_y));

    if (delta_x != 0.0f)
    {
        f32 t = Clamp(t0, (center_x - start_x) / delta_x, t1);
        closest = Min(closest, diamond_distance(start_x, start_y, delta_x, delta_y, t, center_x, center_y));
    }

    if (delta_y != 0.0f)
    {
        f32 t = Clamp(t0, (center_y - start_y) / delta_y, t1);
        closest = Min(closest, diamond_distance(start_x, start_y, delta_x, delta_y, t, center_x, center_y));
    }

    return closest < LOS_DIAMOND_RADIUS + LOS_DIAMOND_MARGIN;
}

static f32 diamond_distance(f32 start_x, f32 start_y, f32 delta_x, f32 delta_y,
                            f32 t, f32 center_x, f32 center_y)
{
    return absolute(start_x + delta_x * t - center_x) + absolute(start_y + delta_y * t - center_y);
}

static f32 absolute(f32 value)
{
    return value < 0.0f ? -value : value;
}
