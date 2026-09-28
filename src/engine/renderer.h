#ifndef RENDERER_H
#define RENDERER_H

#include "mesh.h"
#include "platform.h"
#include "render_types.h"

typedef struct
{
    u32 n_draw_calls;
    u32 n_triangles;
} render_stats_t;

extern render_stats_t g_render_stats;

bool Renderer_Init(platform_window_t *window);
void Renderer_Destroy(void);

bool Renderer_HandleResize(u32 width, u32 height);

window_extent_t Renderer_GetWindowExtent(void);

shader_code_t Renderer_LoadShader(const char *path);

/* the returned handle indexes the global texture array in shaders */
texture_handle_t Renderer_LoadTexture(const char *path, sampler_handle_t sampler);

/* texture from raw pixels (width * height * 4 bytes, copied during the
   call); same handle semantics as Renderer_LoadTexture */
texture_handle_t Renderer_CreateTexture(u32 width, u32 height, const u8 *rgba_data,
                                        sampler_handle_t sampler);
sampler_handle_t Renderer_CreateSampler(void);

/* a texture that a render pass draws into; sampled like any loaded texture */
texture_handle_t Renderer_CreateRenderTexture(u32 width, u32 height,
                                              render_texture_format_t format,
                                              sampler_handle_t sampler);

/* a pass rendering into one or more render textures of equal size; each
   target is either cleared to transparent black or keeps what an earlier
   pass rendered into it this frame. passes render in ascending order before
   the swapchain pass, so any pass can sample the render textures of the
   passes before it. a pipeline writes the first color_output_count targets
   of its pass (default 1) */
renderpass_handle_t Renderer_CreateRenderPass(const renderpass_config_t *config);

pipeline_handle_t Renderer_AddPipeline(renderpass_handle_t pass_handle,
                                       const pipeline_config_t *config);

buffer_object_handle_t Renderer_CreateUniformBuffer(u64 size);

/* storage buffer; like textures, needs no pipeline configuration: shaders
   reach it through its device address (GL_EXT_buffer_reference), passed in
   the push constant per draw. capacity is the initial size; Set/Push grow
   the buffer on demand */
buffer_object_handle_t Renderer_CreateStorageBuffer(u64 capacity);

/* the data is copied into the buffer object's cpu shadow and uploaded to the
   gpu buffers over the next frames; the pointer only needs to stay valid for
   the duration of the call */
bool Renderer_SetBufferObject(buffer_object_handle_t handle, const void *data, u64 size);
bool Renderer_ClearBufferObject(buffer_object_handle_t handle);
bool Renderer_PushBufferObject(buffer_object_handle_t handle, const void *data, u64 size);

/* one mesh per vertex stream the pipeline declares (all sharing one index
   buffer / topology); instance_buffer rides the push constant as a device
   address, BUFFER_OBJECT_HANDLE_INVALID if unused */
void Renderer_DrawMeshes(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                         const void *push_constant_data,
                         buffer_object_handle_t instance_buffer, u32 instance_count,
                         const mesh_handle_t *meshes, u32 mesh_count);

void Renderer_DrawMeshInstanced(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                                const void *push_constant_data,
                                buffer_object_handle_t instance_buffer, u32 instance_count,
                                mesh_handle_t mesh);

void Renderer_BeginFrame();
bool Renderer_EndFrame();

#endif
