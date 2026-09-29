#include "server.h"

#include "array_queue.h"
#include "los.h"
#include "level.h"
#include "level_gen.h"
#include "log.h"
#include "debug_category.h"
#include "memory_arena.h"
#include "rng.h"
#include "rules.h"

#define LEVEL_WIDTH             256
#define LEVEL_HEIGHT            128

#define OUTBOX_CAPACITY         1024

#define REVEAL_RADIUS           (LOS_RADIUS + 1)

typedef struct
{
    arena_t *arena;
    arena_t *run_arena;
    rng_t rng;

    level_t level;
    los_t los;

    i32 player_x;
    i32 player_y;

    array_queue_t outbox;
} server_t;

static server_t g_server;

static void reveal_visible(void);
static void handle_move(const command_move_t *move);
static void send(const event_t *event);

bool Server_Init(u64 seed)
{
    server_t *server = &g_server;

    server->arena = MemoryArena_Create("server-arena");
    server->run_arena = MemoryArena_Create("server-run-arena");
    server->outbox = ArrayQueue_Create(server->arena, sizeof(event_t), OUTBOX_CAPACITY);
    LoS_Init(&server->los, server->arena, LOS_RADIUS);
    Rng_Seed(&server->rng, seed);

    DEBUG(SERVER, "server seed=%lu", seed);

    Level_Init(&server->level, server->run_arena, LEVEL_WIDTH, LEVEL_HEIGHT);

    LevelGen_Generate(&server->level, &server->rng, &server->player_x, &server->player_y);

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

    reveal_visible();

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

static void reveal_visible(void)
{
    server_t *server = &g_server;
    level_t *level = &server->level;

    LoS_Compute(&server->los, level, server->player_x, server->player_y);

    for (i32 y = server->player_y - REVEAL_RADIUS; y <= server->player_y + REVEAL_RADIUS; y++)
    {
        for (i32 x = server->player_x - REVEAL_RADIUS; x <= server->player_x + REVEAL_RADIUS; x++)
        {
            if (!Level_InBounds(level, x, y))
                continue;

            tile_t *tile = Level_GetTile(level, x, y);
            if (tile->flags & FLAG_REVEALED)
                continue;

            // /* also reveal tiles that borders a visible tile */
            // if (!(tile->flags & FLAG_VISIBLE) && !LoS_TouchesVisibleOpenTile(level, x, y))
            //     continue;
            if (!(tile->flags & FLAG_VISIBLE))
                continue;

            tile->flags |= FLAG_REVEALED;
            event_t reveal = {
                .type = TILE_REVEAL,
                .tile_reveal =
                    {
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
        DEBUG(SERVER, "rejected move d=%d,%d", move->dx, move->dy);
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

    reveal_visible();
}

static void send(const event_t *event)
{
    if (!ArrayQueue_Push(&g_server.outbox, event))
    {
        Log(ERROR, "server: outbox full, dropped event type=%u", event->type);
        Assert(false);
    }
}
