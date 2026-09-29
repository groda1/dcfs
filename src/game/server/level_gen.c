#include "level_gen.h"

#include "log.h"
#include "debug_category.h"

#define DEBUG_CATEGORY  DEBUG_CAT_LEVEL_GEN

#define MAX_ROOMS           40
#define ROOM_ATTEMPTS       1000
#define ROOM_MIN_WIDTH      5
#define ROOM_MAX_WIDTH      25
#define ROOM_MIN_HEIGHT     3
#define ROOM_MAX_HEIGHT     13
#define ROOM_MERGE_CHANCE   15
#define EXTRA_JOIN_CHANCE   30

typedef struct
{
    i32 x;
    i32 y;
    i32 width;
    i32 height;
} room_t;

static i32  place_rooms(level_t *level, rng_t *rng, room_t *rooms);
static bool rooms_overlap(const room_t *a, const room_t *b);
static i32  room_distance(const room_t *a, const room_t *b);
static void join_rooms(level_t *level, rng_t *rng, room_t *rooms, i32 room_count);
static void carve_room(level_t *level, const room_t *room);
static void carve_corridor(level_t *level, rng_t *rng, const room_t *a, const room_t *b);
static void carve_line(level_t *level, i32 x0, i32 y0, i32 x1, i32 y1);
static void build_walls(level_t *level);
static i32  random_odd(rng_t *rng, i32 min, i32 max);
static i32  random_odd_in_room_x(rng_t *rng, const room_t *room);
static i32  random_odd_in_room_y(rng_t *rng, const room_t *room);

void LevelGen_Generate(level_t *level, rng_t *rng, i32 *start_x, i32 *start_y)
{
    MemoryZero(level->tiles, sizeof(tile_t) * level->width * level->height);

    room_t rooms[MAX_ROOMS];
    i32 room_count = place_rooms(level, rng, rooms);
    AssertAlways(room_count > 0);

    for (i32 i = 0; i < room_count; i++)
        carve_room(level, &rooms[i]);

    join_rooms(level, rng, rooms, room_count);

    build_walls(level);

    const room_t *start = &rooms[Rng_Range(rng, 0, room_count)];
    *start_x = random_odd_in_room_x(rng, start);
    *start_y = random_odd_in_room_y(rng, start);

    DEBUG("%d rooms, start %d,%d", room_count, *start_x, *start_y);
}

static i32 place_rooms(level_t *level, rng_t *rng, room_t *rooms)
{
    i32 count = 0;

    for (i32 attempt = 0; attempt < ROOM_ATTEMPTS && count < MAX_ROOMS; attempt++)
    {
        room_t room;
        room.width = random_odd(rng, ROOM_MIN_WIDTH, ROOM_MAX_WIDTH);
        room.height = random_odd(rng, ROOM_MIN_HEIGHT, ROOM_MAX_HEIGHT);
        room.x = random_odd(rng, 1, level->width - room.width - 1);
        room.y = random_odd(rng, 1, level->height - room.height - 1);

        i32 overlap_count = 0;
        for (i32 i = 0; i < count; i++)
            if (rooms_overlap(&room, &rooms[i]))
                overlap_count++;

        bool accept = overlap_count == 0
                   || (overlap_count == 1 && Rng_Range(rng, 0, 100) < ROOM_MERGE_CHANCE);
        if (accept)
            rooms[count++] = room;
    }

    return count;
}

static bool rooms_overlap(const room_t *a, const room_t *b)
{
    return a->x < b->x + b->width
        && b->x < a->x + a->width
        && a->y < b->y + b->height
        && b->y < a->y + a->height;
}

static i32 room_distance(const room_t *a, const room_t *b)
{
    i32 dx = (a->x + a->width / 2) - (b->x + b->width / 2);
    i32 dy = (a->y + a->height / 2) - (b->y + b->height / 2);
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

static void join_rooms(level_t *level, rng_t *rng, room_t *rooms, i32 room_count)
{
    bool in_tree[MAX_ROOMS] = {};
    bool joined[MAX_ROOMS][MAX_ROOMS] = {};

    in_tree[0] = true;
    for (i32 added = 1; added < room_count; added++)
    {
        i32 best_from = -1;
        i32 best_to = -1;
        i32 best_distance = I32_MAX;

        for (i32 from = 0; from < room_count; from++)
        {
            if (!in_tree[from])
                continue;

            for (i32 to = 0; to < room_count; to++)
            {
                if (in_tree[to])
                    continue;

                i32 distance = room_distance(&rooms[from], &rooms[to]);
                if (distance < best_distance)
                {
                    best_distance = distance;
                    best_from = from;
                    best_to = to;
                }
            }
        }

        in_tree[best_to] = true;
        joined[best_from][best_to] = true;
        joined[best_to][best_from] = true;
        carve_corridor(level, rng, &rooms[best_from], &rooms[best_to]);
    }

    for (i32 from = 0; from < room_count; from++)
    {
        if (Rng_Range(rng, 0, 100) >= EXTRA_JOIN_CHANCE)
            continue;

        i32 best_to = -1;
        i32 best_distance = I32_MAX;
        for (i32 to = 0; to < room_count; to++)
        {
            if (to == from || joined[from][to])
                continue;

            i32 distance = room_distance(&rooms[from], &rooms[to]);
            if (distance < best_distance)
            {
                best_distance = distance;
                best_to = to;
            }
        }

        if (best_to < 0)
            continue;

        joined[from][best_to] = true;
        joined[best_to][from] = true;
        carve_corridor(level, rng, &rooms[from], &rooms[best_to]);
    }
}

static void carve_room(level_t *level, const room_t *room)
{
    for (i32 y = room->y; y < room->y + room->height; y++)
        for (i32 x = room->x; x < room->x + room->width; x++)
            Level_GetTile(level, x, y)->type = TILE_FLOOR;
}

static void carve_corridor(level_t *level, rng_t *rng, const room_t *a, const room_t *b)
{
    if (rooms_overlap(a, b))
        return;

    i32 ax = random_odd_in_room_x(rng, a);
    i32 ay = random_odd_in_room_y(rng, a);
    i32 bx = random_odd_in_room_x(rng, b);
    i32 by = random_odd_in_room_y(rng, b);

    if (Rng_Range(rng, 0, 2))
    {
        carve_line(level, ax, ay, bx, ay);
        carve_line(level, bx, ay, bx, by);
    }
    else
    {
        carve_line(level, ax, ay, ax, by);
        carve_line(level, ax, by, bx, by);
    }
}

static void carve_line(level_t *level, i32 x0, i32 y0, i32 x1, i32 y1)
{
    i32 dx = (x1 > x0) - (x1 < x0);
    i32 dy = (y1 > y0) - (y1 < y0);

    i32 x = x0;
    i32 y = y0;
    for (;;)
    {
        Level_GetTile(level, x, y)->type = TILE_FLOOR;
        if (x == x1 && y == y1)
            break;
        x += dx;
        y += dy;
    }
}

static void build_walls(level_t *level)
{
    for (i32 y = 0; y < level->height; y++)
    {
        for (i32 x = 0; x < level->width; x++)
        {
            tile_t *tile = Level_GetTile(level, x, y);
            if (tile->type != TILE_EMPTY)
                continue;

            bool next_to_floor = false;
            for (i32 ny = y - 1; ny <= y + 1 && !next_to_floor; ny++)
                for (i32 nx = x - 1; nx <= x + 1 && !next_to_floor; nx++)
                    next_to_floor = Level_InBounds(level, nx, ny)
                                 && Level_GetTile(level, nx, ny)->type == TILE_FLOOR;

            if (next_to_floor)
                tile->type = TILE_WALL;
        }
    }
}

static i32 random_odd(rng_t *rng, i32 min, i32 max)
{
    i32 first = min | 1;
    i32 last = (max & 1) ? max : max - 1;
    Assert(first <= last);

    return first + 2 * Rng_Range(rng, 0, (last - first) / 2 + 1);
}

static i32 random_odd_in_room_x(rng_t *rng, const room_t *room)
{
    return random_odd(rng, room->x, room->x + room->width - 1);
}

static i32 random_odd_in_room_y(rng_t *rng, const room_t *room)
{
    return random_odd(rng, room->y, room->y + room->height - 1);
}
