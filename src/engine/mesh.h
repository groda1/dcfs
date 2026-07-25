#ifndef MESH_H
#define MESH_H

#include "core_math.h"
#include "memory_arena.h"
#include "render_types.h"

#define MESH_INVALID_HANDLE NULL

/* opaque: mesh_t is only completed by the engine-private mesh_internal.h,
   so the mesh data (gpu buffer handles) never leaves the engine */
typedef struct _mesh_t mesh_t;
typedef const mesh_t *mesh_handle_t;

typedef enum
{
    PREDEFINED_MESH_SIMPLE_TRIANGLE   = 1,
    PREDEFINED_MESH_SIMPLE_QUAD,
    PREDEFINED_MESH_NORMALED_QUAD,
    PREDEFINED_MESH_NORMALED_CUBE,
    PREDEFINED_MESH_COLORED_TRIANGLE,
    PREDEFINED_MESH_COLORED_QUAD,
    PREDEFINED_MESH_TEXTURED_QUAD,

    PREDEFINED_MESH_COUNT,
} predefined_mesh_t;

typedef struct _simple_vertex_t simple_vertex_t;
typedef struct _normal_vertex_t normal_vertex_t;
typedef struct _colored_vertex_t colored_vertex_t;
typedef struct _textured_vertex_t textured_vertex_t;
typedef struct _normal_material_vertex_t normal_material_vertex_t;

struct _simple_vertex_t
{
    vec3 position;
};

struct _normal_vertex_t
{
    vec3 position;
    vec3 normal;
};

struct _colored_vertex_t
{
    vec3 position;
    vec3 color;
};

struct _textured_vertex_t
{
    vec3 position;
    vec2 texture_coord;
};

struct _normal_material_vertex_t
{
    vec3 position;
    vec3 normal;
    u32 material;
};

/* one layout per vertex struct, for pipeline_config_t.vertex_layout; the
   pipeline verifies its vertex shader against the layout at creation */
extern const vertex_layout_t VERTEX_LAYOUT_SIMPLE;
extern const vertex_layout_t VERTEX_LAYOUT_NORMAL;
extern const vertex_layout_t VERTEX_LAYOUT_COLORED;
extern const vertex_layout_t VERTEX_LAYOUT_TEXTURED;
extern const vertex_layout_t VERTEX_LAYOUT_NORMAL_MATERIAL;

bool MeshManager_Init();

mesh_handle_t MeshManager_GetPredefinedMesh(predefined_mesh_t mesh);

#endif
