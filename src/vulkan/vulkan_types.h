#ifndef VULKAN_TYPES_H
#define VULKAN_TYPES_H

#include <vulkan/vulkan_core.h>

#include "core.h"
#include "render_types.h"

/* types shared between the vulkan backend modules */

#define MAX_FRAMES_IN_FLIGHT 3

typedef struct _swapchain_t swapchain_t;
typedef struct _draw_command_t draw_command_t;

struct _swapchain_t
{
    VkSwapchainKHR handle;

    VkImage     images[MAX_FRAMES_IN_FLIGHT];
    VkImageView image_views[MAX_FRAMES_IN_FLIGHT];
    VkFormat    format;
    VkExtent2D  extent;
};

struct _draw_command_t
{
    draw_command_t *next;

    renderpass_handle_t pass;
    pipeline_handle_t   pipeline;

    const void *push_constant_data;

    buffer_object_handle_t storage_buffer;

    VkBuffer vertex_buffers[MAX_VERTEX_BINDINGS];
    u32      vertex_buffer_count;
    VkBuffer index_buffer;
    u32      index_count;
    u32      instance_count;
};

#endif
