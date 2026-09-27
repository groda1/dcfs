#include "array_queue.h"

array_queue_t ArrayQueue_Create(arena_t *arena, u64 item_size, u64 capacity)
{
    Assert(arena);
    AssertAlways(item_size > 0 && capacity > 0);

    u8 *items = arena_push_array(arena, u8, item_size * capacity);
    return (array_queue_t){
        .items = items,
        .item_size = item_size,
        .capacity = capacity,
        .head = 0,
        .count = 0,
    };
}

bool ArrayQueue_Push(array_queue_t *queue, const void *item)
{
    if (queue->count == queue->capacity)
        return false;

    u64 tail = (queue->head + queue->count) % queue->capacity;
    MemoryCopy(queue->items + tail * queue->item_size, item, queue->item_size);
    queue->count++;
    return true;
}

bool ArrayQueue_Pop(array_queue_t *queue, void *item_out)
{
    if (queue->count == 0)
        return false;

    MemoryCopy(item_out, queue->items + queue->head * queue->item_size, queue->item_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return true;
}

bool ArrayQueue_Peek(const array_queue_t *queue, void *item_out)
{
    if (queue->count == 0)
        return false;

    MemoryCopy(item_out, queue->items + queue->head * queue->item_size, queue->item_size);
    return true;
}

u64 ArrayQueue_Count(const array_queue_t *queue)
{
    return queue->count;
}
