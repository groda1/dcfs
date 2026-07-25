#ifndef VULKAN_SPIRV_H
#define VULKAN_SPIRV_H

#include "core.h"
#include "render_types.h"

typedef struct
{
    u32             location;
    vertex_format_t format;
} spirv_vertex_input_t;

/* reflects a vertex shader's input variables (location + format) from its
   SPIR-V so pipeline creation can verify them against the configured vertex
   layout. builtins (gl_VertexIndex etc.) are skipped. returns false on a
   malformed blob or an input type the engine has no vertex format for */
bool VulkanSpirv_ReflectVertexInputs(shader_code_t shader, spirv_vertex_input_t *inputs_out,
                                     u32 max_inputs, u32 *input_count_out);

#endif
