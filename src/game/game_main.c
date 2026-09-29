#include "core.h"
#include "log.h"
#include "os_time.h"

#include "client.h"
#include "debug_category.h"
#include "engine_main.h"
#include "game_main.h"
#include "platform.h"
#include "protocol.h"
#include "server.h"


static void pump(void);

#ifdef DEBUG_BUILD

#include "debug_category.h"
#include "engine_debug_category.h"

static void add_debug_categories(void);


#endif

bool Game_Init(platform_window_t *window)
{
#ifdef DEBUG_BUILD
    StaticAssert(CORE_DEBUG_CATEGORY_COUNT <= ENGINE_DEBUG_CATEGORY_FIRST,
             "core debug categories overlap the engine's");
    StaticAssert(ENGINE_DEBUG_CATEGORY_END <= GAME_DEBUG_CATEGORY_FIRST,
             "engine debug categories overlap the game's");

    add_debug_categories();
#endif

    if (!Engine_Init(window))
        return false;

    if (!Client_Init())
        goto error_engine;

    if (!Server_Init(OS_TimeNowNs()))
    //if (!Server_Init(123456))
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

#ifdef DEBUG_BUILD
static void add_debug_categories(void)
{
    Log_AddDebugCategory(string_lit("renderer"),    DEBUG_CAT_RENDERER,     false);
    Log_AddDebugCategory(string_lit("model"),       DEBUG_CAT_MODEL,        false);
    Log_AddDebugCategory(string_lit("animation"),   DEBUG_CAT_ANIMATION,    false);
    Log_AddDebugCategory(string_lit("console"),     DEBUG_CAT_CONSOLE,      true);
    Log_AddDebugCategory(string_lit("validation"),  DEBUG_CAT_VALIDATION,   true);
    Log_AddDebugCategory(string_lit("protocol"),    DEBUG_CAT_PROTOCOL,     false);
    Log_AddDebugCategory(string_lit("client"),      DEBUG_CAT_CLIENT,       true);
    Log_AddDebugCategory(string_lit("server"),      DEBUG_CAT_SERVER,       true);
    Log_AddDebugCategory(string_lit("los"),         DEBUG_CAT_LOS,          true);
    Log_AddDebugCategory(string_lit("level_gen"),   DEBUG_CAT_LEVEL_GEN,    true);
}
#endif
