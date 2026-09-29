#include <stdio.h>

#include "core_string.h"
#include "memory_arena.h"
#include "log.h"

#define LOG_CAPACITY        8192
#define MAX_ENTRY_LENGTH    256

#ifdef DEBUG_BUILD

typedef struct
{
    string name;
    bool enabled;
} debug_category_setting_t;

#define MAX_DEBUG_CATEGORIES    256

#endif // DEBUG_BUILD

typedef struct
{
    arena_t *arena;
    bool stdout;
    bool busy;

    // Entry store
    log_entry_t *entries;
    u64 capacity;
    u64 mask;
    u64 head;
    u64 tail;
    u64 total;

#ifdef DEBUG_BUILD
    debug_category_setting_t debug_categories[MAX_DEBUG_CATEGORIES];
#endif

} log_t;


static log_t *s_logger = NULL;

StaticAssert(IsPow2(LOG_CAPACITY), "bad capacity");

static const char *const severity_map[] =
    {
        [DEBUG_OUTPUT] = "DEBUG",
        [INFO] = "INFO",
        [WARNING] = "WARNING",
        [ERROR] = "ERROR",
        [CVAR] = "CVAR",
};

void Log_Init(void)
{
    if (s_logger)
        return;

    arena_t *arena = MemoryArena_Create("log-arena");
    log_t *l = arena_push(arena, log_t);

    l->arena = arena;
    l->stdout = true;

    l->capacity = LOG_CAPACITY;
    l->head = 0;
    l->tail = 0;
    l->total = 0;
    l->mask = LOG_CAPACITY - 1;

    l->entries = arena_push_array(arena, log_entry_t, LOG_CAPACITY);

    for (u32 i = 0; i < LOG_CAPACITY; i++)
    {
        l->entries[i].text = string_new(arena, MAX_ENTRY_LENGTH);
    }

    s_logger = l;

#ifdef DEBUG_BUILD
    Log_AddDebugCategory(string_lit("arena"), DEBUG_CAT_ARENA, true);
#endif
}

void Log_Destroy(void)
{
    if (!s_logger)
        return;

    MemoryArena_Print(s_logger->arena);
    MemoryArena_Destroy(s_logger->arena);
    s_logger = NULL;
}

static void push_entry(log_severity_t severity, string text)
{
    if (Log_Count() >= (s_logger->capacity - 1))
    {
        s_logger->head++;
        if (s_logger->head == s_logger->capacity)
            s_logger->head = 0;
    }

    log_entry_t *entry = &s_logger->entries[s_logger->tail];

    string_copy(text, &entry->text);

    if (s_logger->stdout)
        printf("[%s] " STR_FMT "\n", severity_map[severity], STR_ARG(text));

    entry->severity = severity;

    s_logger->tail++;
    if (s_logger->tail == s_logger->capacity)
        s_logger->tail = 0;

    s_logger->total++;
}

void Log(log_severity_t severity, const char *log, ...)
{
    if (!s_logger || s_logger->busy)
        return;

    Assert(severity != DEBUG_OUTPUT);

    s_logger->busy = true;
    u64 pos = MemoryArena_Pos(s_logger->arena);

    va_list args;
    va_start(args, log);
    string text = string_fmtv(s_logger->arena, log, args);
    va_end(args);

    push_entry(severity, text);

    MemoryArena_PopTo(s_logger->arena, pos);
    s_logger->busy = false;
}

u64 Log_Count()
{
    return (s_logger->tail - s_logger->head) & s_logger->mask;
}

u64 Log_Total()
{
    return s_logger->total;
}

log_entry_t *Log_Get(u64 index)
{
    if (index >= Log_Count())
       return NULL;

    i64 i = s_logger->tail - index - 1;

    if (i < 0)
    {
        i += s_logger->capacity;
    }

    return &s_logger->entries[i];
}

#ifdef DEBUG_BUILD
void Log_AddDebugCategory(string name, u16 category, bool enabled)
{
    Assert(category < MAX_DEBUG_CATEGORIES);

    s_logger->debug_categories[category] = (debug_category_setting_t){
        .name = string_clone(s_logger->arena, name),
        .enabled = enabled,
    };
}

void Log_SetDebugCategory(u16 category, bool enabled)
{
    Assert(category < MAX_DEBUG_CATEGORIES);

    s_logger->debug_categories[category].enabled = enabled;
}

bool Log_DebugLogEnabled(u16 category)
{
    if (!s_logger || category >= MAX_DEBUG_CATEGORIES)
        return false;

    return s_logger->debug_categories[category].enabled;
}

void _DebugLogImpl(u16 category, const char* file, int line, const char* fmt, ...)
{
    if (!s_logger || s_logger->busy)
        return;

    s_logger->busy = true;
    u64 pos = MemoryArena_Pos(s_logger->arena);

    va_list args;
    va_start(args, fmt);
    string message = string_fmtv(s_logger->arena, fmt, args);
    va_end(args);

    string text = string_fmt(s_logger->arena, "[" STR_FMT "] " STR_FMT " (%s:%d)",
                             STR_ARG(s_logger->debug_categories[category].name), STR_ARG(message),
                             file, line);
    push_entry(DEBUG_OUTPUT, text);

    MemoryArena_PopTo(s_logger->arena, pos);
    s_logger->busy = false;
}
#else
void _DebugLogImpl(u16 category, const char* file, int line, const char* fmt, ...)
{
    (void)category;
    (void)file;
    (void)line;
    (void)fmt;
}
#endif // DEBUG_BUILD
