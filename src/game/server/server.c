#include "server.h"

#include "array_queue.h"
#include "level.h"
#include "log.h"
#include "memory_arena.h"
#include "rng.h"
#include "rules.h"

#define LEVEL_WIDTH             256
#define LEVEL_HEIGHT            128
#define LEVEL_WALL_SEGMENTS     300
#define LEVEL_WALL_MIN_LENGTH   3
#define LEVEL_WALL_MAX_LENGTH   12

#define OUTBOX_CAPACITY         1024

#define REVEAL_RADIUS           3

typedef struct
{
    arena_t *arena;
    arena_t *run_arena;
    rng_t rng;

    level_t level;

    i32 player_x;
    i32 player_y;

    array_queue_t outbox;
} server_t;

static server_t g_server;

static void generate_level(void);
static void reveal_around_player(void);
static void handle_move(const command_move_t *move);
static void send(const event_t *event);

bool Server_Init(u64 seed)
{
    server_t *server = &g_server;

    server->arena = MemoryArena_Create("server-arena");
    server->run_arena = MemoryArena_Create("server-run-arena");
    server->outbox = ArrayQueue_Create(server->arena, sizeof(event_t), OUTBOX_CAPACITY);
    Rng_Seed(&server->rng, seed);
    Log(INFO, "server seed=%lu", seed);

    Level_Init(&server->level, server->run_arena, LEVEL_WIDTH, LEVEL_HEIGHT);

    server->player_x = LEVEL_WIDTH / 2;
    server->player_y = LEVEL_HEIGHT / 2;

    generate_level();

    event_t init = {
        .type = LEVEL_INIT,
        .level_init = {
            .level_width = server->level.width,
            .level_height = server->level.height,
        },
    };
    send(&init);

    event_t placed = {
        .type = PLAYER_MOVED,
        .player_moved = {
            .x = (u16)server->player_x,
            .y = (u16)server->player_y,
        },
    };
    send(&placed);

    reveal_around_player();

    event_t start = {
        .type = RUN_START,
    };
    send(&start);

    return true;
}

void Server_Destroy(void)
{
    MemoryArena_Destroy(g_server.run_arena);
    MemoryArena_Destroy(g_server.arena);
}

void Server_HandleCommand(const command_t *command)
{
    Protocol_LogCommand("server", command);

    switch (command->type)
    {
        case COMMAND_MOVE:
            handle_move(&command->move);
            break;
    }

    event_t done = {
        .type = TURN_DONE,
        .turn_done = {
            .sequence = command->sequence,
        },
    };
    send(&done);
}

bool Server_PollEvent(event_t *event)
{
    return ArrayQueue_Pop(&g_server.outbox, event);
}

static void generate_level(void)
{
    server_t *server = &g_server;
    level_t *level = &server->level;

    for (i32 y = 0; y < level->height; y++)
    {
        for (i32 x = 0; x < level->width; x++)
        {
            bool border = x == 0 || y == 0 || x == level->width - 1 || y == level->height - 1;
            Level_GetTile(level, x, y)->type = border ? TILE_WALL : TILE_FLOOR;
        }
    }

    for (i32 i = 0; i < LEVEL_WALL_SEGMENTS; i++)
    {
        i32 x = Rng_Range(&server->rng, 1, level->width - 1);
        i32 y = Rng_Range(&server->rng, 1, level->height - 1);
        i32 length = Rng_Range(&server->rng, LEVEL_WALL_MIN_LENGTH, LEVEL_WALL_MAX_LENGTH + 1);
        bool horizontal = Rng_Range(&server->rng, 0, 2);

        for (i32 j = 0; j < length && Level_InBounds(level, x, y); j++)
        {
            Level_GetTile(level, x, y)->type = TILE_WALL;
            if (horizontal)
                x++;
            else
                y++;
        }
    }

    Level_GetTile(level, server->player_x, server->player_y)->type = TILE_FLOOR;
}

static void reveal_around_player(void)
{
    server_t *server = &g_server;
    level_t *level = &server->level;

    for (i32 y = server->player_y - REVEAL_RADIUS; y <= server->player_y + REVEAL_RADIUS; y++)
    {
        for (i32 x = server->player_x - REVEAL_RADIUS; x <= server->player_x + REVEAL_RADIUS; x++)
        {
            if (!Level_InBounds(level, x, y))
                continue;

            tile_t *tile = Level_GetTile(level, x, y);
            if (tile->flags & FLAG_REVEALED)
                continue;

            tile->flags |= FLAG_REVEALED;

            event_t reveal = {
                .type = TILE_REVEAL,
                .tile_reveal = {
                    .x = (u16)x,
                    .y = (u16)y,
                    .tile = tile->type,
                },
            };
            send(&reveal);
        }
    }
}

static void handle_move(const command_move_t *move)
{
    server_t *server = &g_server;

    bool valid_step = (move->dx != 0 || move->dy != 0)
                   && move->dx >= -1 && move->dx <= 1
                   && move->dy >= -1 && move->dy <= 1;

    i32 x = server->player_x + move->dx;
    i32 y = server->player_y + move->dy;

    if (!valid_step || !Rules_IsWalkable(&server->level, x, y))
    {
        Log(DEBUG, "server: rejected move d=%d,%d", move->dx, move->dy);
        return;
    }

    server->player_x = x;
    server->player_y = y;

    event_t moved = {
        .type = PLAYER_MOVED,
        .player_moved = {
            .x = (u16)server->player_x,
            .y = (u16)server->player_y,
        },
    };
    send(&moved);

    reveal_around_player();
}

static void send(const event_t *event)
{
    if (!ArrayQueue_Push(&g_server.outbox, event))
    {
        Log(ERROR, "server: outbox full, dropped event type=%u", event->type);
        Assert(false);
    }
}
