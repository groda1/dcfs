#ifndef VULKAN_PASS_H
#define VULKAN_PASS_H

#include <vulkan/vulkan_core.h>

#include "memory_arena.h"
#include "render_types.h"
#include "vulkan_types.h"

bool VulkanPass_Init(arena_t *frame_arena);
bool VulkanPass_Destroy();
bool VulkanPass_CreateSwapchainPass(swapchain_t *swapchain);
bool VulkanPass_RecreateSwapchainPass(swapchain_t *swapchain);

renderpass_handle_t VulkanPass_CreateImagePass(const renderpass_config_t *config);
pipeline_handle_t VulkanPass_AddPipeline(renderpass_handle_t pass_handle,
                                         const pipeline_config_t *config);

void VulkanPass_BeginFrame();
void VulkanPass_AddDrawCommand(const draw_command_t *draw_command);
bool VulkanPass_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);

#endif
