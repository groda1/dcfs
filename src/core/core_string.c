#include <stdio.h>
#include <stdarg.h>
#include "core.h"
#include "memory_arena.h"

#include "core_string.h"


string string_new(arena_t *arena, u64 capacity)
{
    return (string){
        .str = arena_push_array(arena, u8, capacity),
        .len = 0,
        .cap = capacity,
    };
}

string string_from_l(const char *str, u64 len)
{
    return (string){
        .str = (u8 *)str,
        .len = len,
        .cap = 0,
    };
}

string string_from(const char *str)
{
    return (string){
        .str = (u8 *)str,
        .len = strlen(str),
        .cap = 0,
    };
}

string string_fmt(arena_t *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    string s = string_fmtv(arena, fmt, args);
    va_end(args);
    return s;
}

string string_fmtv(arena_t *arena, const char *fmt, va_list args)
{
    va_list measure;
    va_copy(measure, args);
    int n = vsnprintf(NULL, 0, fmt, measure);
    va_end(measure);

    u64 len = n > 0 ? (u64)n : 0;

    string s;
    s.cap = len + 1;
    s.str = arena_push_array_no_zero(arena, u8, s.cap);

    va_list write;
    va_copy(write, args);
    vsnprintf((char *)s.str, s.cap, fmt, write);
    va_end(write);

    s.str[len] = 0;
    s.len = len;
    return s;
}

string string_fmt_a(arena_t arena, string *s, ...);

string string_clone(arena_t *arena, string src)
{
    string new = string_new(arena, src.len + 1);
    string_copy(src, &new);
    return new;
}

bool string_match(string s1, string s2)
{
    if (s1.len != s2.len)
        return false;
    return MemoryMatch(s1.str, s2.str, s1.len);
}

void string_copy(string src, string *dst)
{
    Assert(dst);

    u64 len = Min(strlen((char *)src.str), dst->cap - 1);

    MemoryCopy(dst->str, src.str, len);
    dst->str[len] = 0;
    dst->len = len;
}
