#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "core.h"

#include "render_types.h"
#include "vulkan_types.h"

typedef struct _pipeline_t pipeline_t;

struct _pipeline_t
{
    VkPipeline          vk_pipeline;
    VkPipelineLayout    layout;
    u32                 push_constant_size;
    u32                 vertex_binding_count; /* draws must supply this many meshes */

    VkDescriptorSetLayout   descriptor_set_layout;
    VkDescriptorPool        descriptor_pool;
    VkDescriptorSet         descriptor_sets[MAX_FRAMES_IN_FLIGHT];
};

bool VulkanPipeline_Create(const VkFormat *color_formats, u32 color_format_count,
                           VkFormat depth_format, const pipeline_config_t *config,
                           pipeline_t *pipeline_out);
void VulkanPipeline_Destroy(pipeline_t *pipeline);

#endif
