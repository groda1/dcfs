#ifndef ARRAY_QUEUE_H
#define ARRAY_QUEUE_H

#include "core.h"
#include "memory_arena.h"

typedef struct
{
    u8 *items;
    u64 item_size;
    u64 capacity;
    u64 head;
    u64 count;
} array_queue_t;

array_queue_t ArrayQueue_Create(arena_t *arena, u64 item_size, u64 capacity);

bool ArrayQueue_Push(array_queue_t *queue, const void *item);
bool ArrayQueue_Pop(array_queue_t *queue, void *item_out);
bool ArrayQueue_Peek(const array_queue_t *queue, void *item_out);

u64 ArrayQueue_Count(const array_queue_t *queue);

#endif
