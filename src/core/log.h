#ifndef LOG_H
#define LOG_H

#include "core.h"
#include "core_string.h"
#include "core_debug_category.h"

typedef enum
{
    DEBUG_OUTPUT = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CVAR = 4,
} log_severity_t;

typedef struct
{
    log_severity_t severity;
    string text;
} log_entry_t;


void Log_Init(void);
void Log_Destroy(void);

void Log(log_severity_t severity, const char *log, ...) AttributePrintf(2, 3);

u64 Log_Count();
u64 Log_Total();
log_entry_t *Log_Get(u64 index);


void _DebugLogImpl(u16 category, const char* file, int line, const char* fmt, ...) AttributePrintf(4, 5);

#ifdef DEBUG_BUILD
void Log_AddDebugCategory(string name, u16 category, bool enabled);
void Log_SetDebugCategory(u16 category, bool enabled);
bool Log_DebugLogEnabled(u16 category);

#define DEBUG_CAT(cat, ...)                                            \
  do                                                                   \
  {                                                                    \
      if (Log_DebugLogEnabled(cat))                                    \
          _DebugLogImpl((cat), __FILE_NAME__, __LINE__, __VA_ARGS__);  \
    } while (0)
#else
#define DEBUG_CAT(cat, ...)                                            \
    do {                                                               \
        if (0)                                                         \
            _DebugLogImpl((cat), __FILE_NAME__, __LINE__, __VA_ARGS__);\
    } while (0)
#endif // DEBUG_BUILD

#define DEBUG(...) DEBUG_CAT(DEBUG_CATEGORY, __VA_ARGS__)


#endif
