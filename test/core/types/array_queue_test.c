#include <criterion/criterion.h>

#include "core.h"
#include "array_queue.h"
#include "memory_arena.h"

typedef struct
{
    u32 a;
    u16 b;
} item_t;

Test(array_queue, fifo_order)
{
    arena_t *arena = MemoryArena_Create("test_arena");
    array_queue_t queue = ArrayQueue_Create(arena, sizeof(item_t), 8);

    for (u32 i = 0; i < 5; i++)
    {
        item_t item = { .a = i, .b = (u16)(i * 2) };
        cr_expect(ArrayQueue_Push(&queue, &item));
    }
    cr_expect(ArrayQueue_Count(&queue) == 5);

    for (u32 i = 0; i < 5; i++)
    {
        item_t item;
        cr_expect(ArrayQueue_Pop(&queue, &item));
        cr_expect(item.a == i && item.b == i * 2);
    }

    item_t item;
    cr_expect(!ArrayQueue_Pop(&queue, &item), "pop on empty queue must fail");

    MemoryArena_Destroy(arena);
}

Test(array_queue, full_and_wraparound)
{
    arena_t *arena = MemoryArena_Create("test_arena");
    array_queue_t queue = ArrayQueue_Create(arena, sizeof(u32), 4);

    u32 next_push = 0;
    u32 next_pop = 0;

    for (u32 round = 0; round < 10; round++)
    {
        while (ArrayQueue_Count(&queue) < 4)
        {
            cr_expect(ArrayQueue_Push(&queue, &next_push));
            next_push++;
        }
        cr_expect(!ArrayQueue_Push(&queue, &next_push), "push on full queue must fail");

        for (u32 i = 0; i < 3; i++)
        {
            u32 value;
            cr_expect(ArrayQueue_Pop(&queue, &value));
            cr_expect(value == next_pop, "expected %u got %u", next_pop, value);
            next_pop++;
        }
    }

    MemoryArena_Destroy(arena);
}

Test(array_queue, peek)
{
    arena_t *arena = MemoryArena_Create("test_arena");
    array_queue_t queue = ArrayQueue_Create(arena, sizeof(u32), 4);

    u32 value;
    cr_expect(!ArrayQueue_Peek(&queue, &value), "peek on empty queue must fail");

    for (u32 i = 10; i < 13; i++)
        ArrayQueue_Push(&queue, &i);

    cr_expect(ArrayQueue_Peek(&queue, &value));
    cr_expect(value == 10);
    cr_expect(ArrayQueue_Count(&queue) == 3, "peek must not remove the item");

    ArrayQueue_Pop(&queue, &value);
    cr_expect(ArrayQueue_Peek(&queue, &value));
    cr_expect(value == 11);

    MemoryArena_Destroy(arena);
}
