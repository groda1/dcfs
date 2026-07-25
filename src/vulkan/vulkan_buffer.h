#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include <vulkan/vulkan_core.h>

#include "core.h"
#include "memory_arena.h"
#include "render_types.h"

/* the pool/queue pair is used for the synchronous static buffer uploads */
bool VulkanBuffer_Init(VkCommandPool command_pool, VkQueue submit_queue);
void VulkanBuffer_Destroy();

/* device-local mesh data, uploaded synchronously during the call; owned by
   the buffer registry, destroyed with it */
VkBuffer VulkanBuffer_CreateStaticVertex(const void *vertices, u64 size);
VkBuffer VulkanBuffer_CreateStaticIndex(const u32 *indices, u32 index_count);

buffer_object_handle_t VulkanBuffer_CreateObject(arena_t *arena, u64 capacity,
                                                 buffer_object_type_t type);
bool VulkanBuffer_SetObjectData(buffer_object_handle_t handle, const void *data, u64 size);
bool VulkanBuffer_ClearObjectData(buffer_object_handle_t handle);
bool VulkanBuffer_PushObjectData(buffer_object_handle_t handle, const void *data, u64 size);

VkBuffer VulkanBuffer_GetDeviceBuffer(buffer_object_handle_t handle, u32 frame_index);
u64 VulkanBuffer_GetObjectCapacity(buffer_object_handle_t handle);

/* BO_STORAGE only: the buffer's device address for the given frame in
   flight, for shaders using GL_EXT_buffer_reference */
VkDeviceAddress VulkanBuffer_GetDeviceAddress(buffer_object_handle_t handle, u32 frame_index);

bool VulkanBuffer_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);

#endif
