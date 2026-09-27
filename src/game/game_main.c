#include "core.h"
#include "log.h"
#include "os_time.h"

#include "client.h"
#include "engine_main.h"
#include "game_main.h"
#include "platform.h"
#include "protocol.h"
#include "server.h"

static void pump(void);

bool Game_Init(platform_window_t *window)
{
    if (!Engine_Init(window))
        return false;

    if (!Client_Init())
        goto error_engine;

    if (!Server_Init(OS_TimeNowNs()))
        goto error_client;

    return true;

error_client:
    Client_Destroy();
error_engine:
    Engine_Destroy();
    return false;
}

void Game_Destroy(void)
{
    Server_Destroy();
    Engine_Destroy();
    Client_Destroy();
}

void Game_HandleKeyDown(key_code_t key)
{
    if (Engine_HandleKeyDown(key) == KEY_EVENT_CONSUMED)
        return;

    Client_HandleKeyDown(key);
}

void Game_HandleKeyUp(key_code_t key)
{
    Engine_HandleKeyUp(key);
}

void Game_HandleResize(u32 width, u32 height)
{
    Engine_HandleResize(width, height);
}

void Game_Tick(void)
{
    f32 delta_time = Engine_BeginFrame();

    pump();
    Client_Update(delta_time);

    Engine_EndFrame();
}

static void pump(void)
{
    command_t command;
    while (Client_PollCommand(&command))
        Server_HandleCommand(&command);

    event_t event;
    while (Server_PollEvent(&event))
        Client_HandleEvent(&event);
}
