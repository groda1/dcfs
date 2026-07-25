#include <stdio.h>

#include "core.h"
#include "file.h"
#include "log.h"
#include "mesh.h"
#include "os_path.h"
#include "image.h"
#include "mesh_internal.h"
#include "renderer.h"

#include "vulkan_buffer.h"
#include "vulkan_pass.h"
#include "vulkan_renderer.h"
#include "vulkan_texture.h"
#include "vulkan_types.h"

#define MAX_RESOURCE_PATH 512

render_stats_t g_render_stats = {};

extern arena_t *g_engine_arena;

bool Renderer_Init(platform_window_t *window)
{
    Assert(g_engine_arena != NULL);

    return VulkanRenderer_Init(g_engine_arena, window);
}

void Renderer_Destroy(void)
{
    VulkanRenderer_Destroy();
}

bool Renderer_HandleResize(u32 width, u32 height)
{
    return VulkanRenderer_HandleResize(width, height);
}

shader_code_t Renderer_LoadShader(const char *path)
{
    Assert(g_engine_arena != NULL);

    char full_path[MAX_RESOURCE_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", OS_GetBasePath(), path);

    shader_code_t shader = {0};
    shader.code = File_Read(g_engine_arena, full_path, &shader.size);

    return shader;
}

texture_handle_t Renderer_LoadTexture(const char *path, sampler_handle_t sampler)
{
    char full_path[MAX_RESOURCE_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", OS_GetBasePath(), path);

    image_t image;
    if (!Image_Load(full_path, &image))
        return TEXTURE_HANDLE_INVALID;

    texture_handle_t texture = VulkanTexture_Create(image.width, image.height,
                                                    image.data, sampler);
    Image_Unload(&image);

    return texture;
}

texture_handle_t Renderer_CreateTexture(u32 width, u32 height, const u8 *rgba_data,
                                        sampler_handle_t sampler)
{
    return VulkanTexture_Create(width, height, rgba_data, sampler);
}

sampler_handle_t Renderer_CreateSampler(void)
{
    return VulkanTexture_CreateSampler();
}

texture_handle_t Renderer_CreateRenderTexture(u32 width, u32 height, sampler_handle_t sampler)
{
    return VulkanTexture_CreateRenderTarget(width, height, sampler);
}

renderpass_handle_t Renderer_CreateRenderPass(texture_handle_t target_texture, u32 pass_order)
{
    return VulkanPass_CreateImagePass(target_texture, pass_order);
}

window_extent_t Renderer_GetWindowExtent(void)
{
    VkExtent2D extent = VulkanRenderer_GetExtent();

    return (window_extent_t){
        .width = extent.width,
        .height = extent.height,
    };
}

pipeline_handle_t Renderer_AddPipeline(renderpass_handle_t pass_handle,
                                       const pipeline_config_t *config)
{
    return VulkanPass_AddPipeline(pass_handle, config);
}

buffer_object_handle_t Renderer_CreateUniformBuffer(u64 size)
{
    return VulkanBuffer_CreateObject(g_engine_arena, size, BO_UNIFORM);
}

buffer_object_handle_t Renderer_CreateStorageBuffer(u64 capacity)
{
    return VulkanBuffer_CreateObject(g_engine_arena, capacity, BO_STORAGE);
}

bool Renderer_SetBufferObject(buffer_object_handle_t handle, const void *data, u64 size)
{
    return VulkanBuffer_SetObjectData(handle, data, size);
}

bool Renderer_ClearBufferObject(buffer_object_handle_t handle)
{
    return VulkanBuffer_ClearObjectData(handle);
}
bool Renderer_PushBufferObject(buffer_object_handle_t handle, const void *data, u64 size)
{
    return VulkanBuffer_PushObjectData(handle, data, size);
}

void Renderer_DrawMeshes(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                         const void *push_constant_data,
                         buffer_object_handle_t instance_buffer, u32 instance_count,
                         const mesh_handle_t *meshes, u32 mesh_count)
{
    Assert(mesh_count >= 1 && mesh_count <= MAX_VERTEX_BINDINGS);

    draw_command_t draw_command = {
        .pass = pass_handle,
        .pipeline = pipeline,
        .push_constant_data = push_constant_data,
        .storage_buffer = instance_buffer,
        .vertex_buffer_count = mesh_count,
        .index_buffer = meshes[0]->index_buffer,
        .index_count = meshes[0]->index_count,
        .instance_count = instance_count,
    };

    for (u32 i = 0; i < mesh_count; i++)
    {
        Assert(meshes[i] != MESH_INVALID_HANDLE);
        /* streams are morph targets of one mesh; their topologies must match */
        Assert(meshes[i]->index_count == meshes[0]->index_count);

        draw_command.vertex_buffers[i] = meshes[i]->vertex_buffer;
    }

    g_render_stats.n_draw_calls++;
    g_render_stats.n_triangles += instance_count * (meshes[0]->index_count / 3);

    VulkanPass_AddDrawCommand(&draw_command);
}

void Renderer_DrawMeshInstanced(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                                const void *push_constant_data,
                                buffer_object_handle_t instance_buffer, u32 instance_count,
                                mesh_handle_t mesh)
{
    Renderer_DrawMeshes(pass_handle, pipeline, push_constant_data, instance_buffer,
                        instance_count, &mesh, 1);
}

void Renderer_BeginFrame()
{
    MemoryZeroItem(&g_render_stats);

    VulkanRenderer_BeginFrame();
}

bool Renderer_EndFrame()
{
    return VulkanRenderer_EndFrame();
}
