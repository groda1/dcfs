#ifndef SERVER_H
#define SERVER_H

#include "core.h"
#include "protocol.h"

bool Server_Init(u64 seed);
void Server_Destroy(void);

void Server_HandleCommand(const command_t *command);
bool Server_PollEvent(event_t *event);

#endif
