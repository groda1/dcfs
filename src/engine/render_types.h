#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

#include "core.h"

/* engine-side rendering types; implemented by the vulkan renderer */

#define SWAPCHAIN_PASS_HANDLE   U32_MAX

/* 0 is the invalid value for every handle type (handles are 1-based indices
   into their registries), so zero-initialized structs hold invalid handles */
#define RENDERPASS_HANDLE_INVALID 0
#define PIPELINE_HANDLE_INVALID 0
#define BUFFER_OBJECT_HANDLE_INVALID 0
#define TEXTURE_HANDLE_INVALID  0
#define SAMPLER_HANDLE_INVALID  0

#define MAX_VERTEX_ATTRIBUTES 8
#define MAX_VERTEX_BINDINGS   3
#define MAX_UNIFORM_BINDINGS  4

typedef u32 renderpass_handle_t;
typedef u32 pipeline_handle_t;
typedef u32 buffer_object_handle_t;
typedef u32 texture_handle_t;
typedef u32 sampler_handle_t;

typedef struct
{
    u32 width;
    u32 height;
} window_extent_t;

typedef enum
{
    VERTEX_FORMAT_F32 = 0,
    VERTEX_FORMAT_F32X2,
    VERTEX_FORMAT_F32X3,
    VERTEX_FORMAT_F32X4,
    VERTEX_FORMAT_U32,
} vertex_format_t;

typedef struct
{
    vertex_format_t format;
    u32             offset;
} vertex_attribute_t;

/* describes one vertex struct; declared by the engine next to the struct
   (see mesh.h). shader input locations are implicit: attribute i is
   location i, and each extra vertex stream continues the numbering */
typedef struct
{
    u32                stride;
    u32                attribute_count;
    vertex_attribute_t attributes[MAX_VERTEX_ATTRIBUTES];
} vertex_layout_t;

typedef struct
{
    u8  *code;
    u64 size;
} shader_code_t;

typedef enum
{
    BO_UNIFORM,
    BO_STORAGE,
} buffer_object_type_t;

typedef enum
{
    UNIFORM_STAGE_VERTEX,
    UNIFORM_STAGE_FRAGMENT,
} uniform_stage_t;

typedef struct
{
    u32                     binding;
    buffer_object_handle_t  buffer_object;
    uniform_stage_t         stage;
} uniform_binding_t;

typedef struct _pipeline_config_t pipeline_config_t;
struct _pipeline_config_t
{
    const char *name;
    shader_code_t vertex_shader;
    shader_code_t fragment_shader;

    u32 push_constant_size; // 0 = no push constants; visible to vertex and fragment stages

    /* the layout of the vertex buffer(s) this pipeline draws; debug builds
       check the vertex shader's inputs against it at creation and fail the
       pipeline on mismatch */
    const vertex_layout_t *vertex_layout;
    u32 vertex_streams; /* 0/1 = one vertex buffer; N = N identical streams
                           (draw calls supply N meshes, shader locations
                           continue across streams) */

    u32 uniform_binding_count;
    uniform_binding_t uniform_bindings[MAX_UNIFORM_BINDINGS];

    bool alpha_blending;
    bool disable_depth_test;

    // TODO vertex topology (always triangle list for now)
};

typedef struct
{
    u64 _instance_data_address; /* renderer-internal: overwritten with the
                                   storage buffer's device address at bake —
                                   never set this manually */
} sbo_push_constant_t;

#endif
