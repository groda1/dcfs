#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <vulkan/vulkan_core.h>

#include "memory_arena.h"
#include "platform.h"

#include "render_types.h"
#include "vulkan_types.h"

typedef struct _vk_renderer_t vk_renderer_t;

/* device and frame lifecycle only; resources go through the subsystem
   modules directly (vulkan_pass, vulkan_buffer, vulkan_texture) */

bool VulkanRenderer_Init(arena_t *arena, platform_window_t *window);
bool VulkanRenderer_Destroy();

bool VulkanRenderer_HandleResize(u32 width, u32 height);

void VulkanRenderer_BeginFrame();
bool VulkanRenderer_EndFrame();
void VulkanRenderer_WaitIdle();

VkExtent2D VulkanRenderer_GetExtent();

#endif
