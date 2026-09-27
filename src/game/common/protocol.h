#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "core.h"


typedef enum
{
    COMMAND_MOVE,
} command_type_t;

// Step one tile in a direction
// client -> server
typedef struct
{
    i8 dx;
    i8 dy;
} command_move_t;

// Player intent, every command advances the game one turn
// client -> server
typedef struct
{
    command_type_t type;
    u32 sequence;

    union
    {
        command_move_t move;
    };
} command_t;


typedef enum
{
    RUN_START,      // initial state is complete, the run is live (no payload)
    LEVEL_INIT,
    TILE_REVEAL,
    PLAYER_MOVED,
    TURN_DONE,
    //NPC_REVEAL,
    //NPC_MOVE,
} event_type_t;

// Initial data on new level
//  server -> client
typedef struct
{
    u16 level_width;
    u16 level_height;
} event_level_init_t;

// New tile revealed on level
// server -> client
typedef struct
{
    u16 x;
    u16 y;
    u16 tile;
} event_tile_reveal_t;

// Player is now at this position
// server -> client
typedef struct
{
    u16 x;
    u16 y;
} event_player_moved_t;

// Command fully resolved, the world has advanced until the player can act again
// server -> client
typedef struct
{
    u32 sequence;
} event_turn_done_t;

typedef struct
{
    event_type_t type;

    union
    {
        event_level_init_t level_init;
        event_tile_reveal_t tile_reveal;
        event_player_moved_t player_moved;
        event_turn_done_t turn_done;
    };
} event_t;

void Protocol_LogCommand(const char *receiver, const command_t *command);
void Protocol_LogEvent(const char *receiver, const event_t *event);

#endif
