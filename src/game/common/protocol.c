#include "protocol.h"
#include "log.h"

void Protocol_LogCommand(const char *receiver, const command_t *command)
{
    switch (command->type)
    {
        case COMMAND_MOVE:
            Log(DEBUG, "%s: recv COMMAND_MOVE seq=%u d=%d,%d", receiver,
                command->sequence, command->move.dx, command->move.dy);
            break;
        default:
            Log(DEBUG, "%s: recv unknown command type=%u seq=%u", receiver,
                command->type, command->sequence);
            break;
    }
}

void Protocol_LogEvent(const char *receiver, const event_t *event)
{
    switch (event->type)
    {
        case RUN_START:
            Log(DEBUG, "%s: recv RUN_START", receiver);
            break;
        case LEVEL_INIT:
            Log(DEBUG, "%s: recv LEVEL_INIT size=%ux%u", receiver,
                event->level_init.level_width, event->level_init.level_height);
            break;
        case TILE_REVEAL:
            Log(DEBUG, "%s: recv TILE_REVEAL %u,%u tile=%u", receiver,
                event->tile_reveal.x, event->tile_reveal.y, event->tile_reveal.tile);
            break;
        case PLAYER_MOVED:
            Log(DEBUG, "%s: recv PLAYER_MOVED %u,%u", receiver,
                event->player_moved.x, event->player_moved.y);
            break;
        case TURN_DONE:
            Log(DEBUG, "%s: recv TURN_DONE seq=%u", receiver, event->turn_done.sequence);
            break;
        default:
            Log(DEBUG, "%s: recv unknown event type=%u", receiver, event->type);
            break;
    }
}
