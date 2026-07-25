#ifndef MESH_INTERNAL_H
#define MESH_INTERNAL_H

#include <vulkan/vulkan_core.h>

#include "core.h"
#include "mesh.h"

struct _mesh_t
{
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    u32 index_count;
};

/* uploads the vertex/index data and returns an engine-arena mesh, NULL on
   failure; the gpu buffers live until renderer shutdown */
mesh_t *Mesh_Create(const void *vertices, u64 vertices_size, const u32 *indices,
                    u32 index_count);

#endif
