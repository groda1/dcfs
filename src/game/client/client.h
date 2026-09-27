#ifndef CLIENT_H
#define CLIENT_H

#include "core.h"
#include "platform.h"
#include "protocol.h"

bool Client_Init(void);
void Client_Destroy(void);

void Client_HandleKeyDown(key_code_t key);
void Client_Update(f32 delta_time);

void Client_HandleEvent(const event_t *event);
bool Client_PollCommand(command_t *command);

#endif
