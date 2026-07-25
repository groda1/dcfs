#include "core.h"
#include "log.h"
#include "memory_arena.h"

#include "mesh.h"
#include "mesh_internal.h"
#include "vulkan_buffer.h"

/* the layouts double as the size contract between the C structs and the
   shaders; a mismatch here fails pipeline creation, not rendering */
StaticAssert(sizeof(simple_vertex_t) == 12, "simple_vertex_t is not tightly packed");
StaticAssert(sizeof(normal_vertex_t) == 24, "normal_vertex_t is not tightly packed");
StaticAssert(sizeof(colored_vertex_t) == 24, "colored_vertex_t is not tightly packed");
StaticAssert(sizeof(textured_vertex_t) == 20, "textured_vertex_t is not tightly packed");
StaticAssert(sizeof(normal_material_vertex_t) == 28,
             "normal_material_vertex_t is not tightly packed");

const vertex_layout_t VERTEX_LAYOUT_SIMPLE = {
    .stride = sizeof(simple_vertex_t),
    .attribute_count = 1,
    .attributes = {
        {VERTEX_FORMAT_F32X3, offsetof(simple_vertex_t, position)},
    },
};

const vertex_layout_t VERTEX_LAYOUT_NORMAL = {
    .stride = sizeof(normal_vertex_t),
    .attribute_count = 2,
    .attributes = {
        {VERTEX_FORMAT_F32X3, offsetof(normal_vertex_t, position)},
        {VERTEX_FORMAT_F32X3, offsetof(normal_vertex_t, normal)},
    },
};

const vertex_layout_t VERTEX_LAYOUT_COLORED = {
    .stride = sizeof(colored_vertex_t),
    .attribute_count = 2,
    .attributes = {
        {VERTEX_FORMAT_F32X3, offsetof(colored_vertex_t, position)},
        {VERTEX_FORMAT_F32X3, offsetof(colored_vertex_t, color)},
    },
};

const vertex_layout_t VERTEX_LAYOUT_TEXTURED = {
    .stride = sizeof(textured_vertex_t),
    .attribute_count = 2,
    .attributes = {
        {VERTEX_FORMAT_F32X3, offsetof(textured_vertex_t, position)},
        {VERTEX_FORMAT_F32X2, offsetof(textured_vertex_t, texture_coord)},
    },
};

const vertex_layout_t VERTEX_LAYOUT_NORMAL_MATERIAL = {
    .stride = sizeof(normal_material_vertex_t),
    .attribute_count = 3,
    .attributes = {
        {VERTEX_FORMAT_F32X3, offsetof(normal_material_vertex_t, position)},
        {VERTEX_FORMAT_F32X3, offsetof(normal_material_vertex_t, normal)},
        {VERTEX_FORMAT_U32, offsetof(normal_material_vertex_t, material)},
    },
};

typedef struct
{
    const mesh_t *predefined[PREDEFINED_MESH_COUNT];
} mesh_collection_t;

static mesh_collection_t *s_meshes;

extern arena_t *g_engine_arena;

static void load_predefined_meshes(void);

bool MeshManager_Init()
{
    Assert(g_engine_arena != NULL);

    s_meshes = arena_push(g_engine_arena, mesh_collection_t);

    load_predefined_meshes();

    Log(INFO, "Mesh manager initialized");
    return true;
}

mesh_handle_t MeshManager_GetPredefinedMesh(predefined_mesh_t mesh)
{
    Assert(mesh > 0 && mesh < PREDEFINED_MESH_COUNT);

    const mesh_t *predefined = s_meshes->predefined[mesh];
    AssertAlways(predefined != NULL);

    return predefined;
}

mesh_t *Mesh_Create(const void *vertices, u64 vertices_size, const u32 *indices,
                    u32 index_count)
{
    VkBuffer vertex_buffer = VulkanBuffer_CreateStaticVertex(vertices, vertices_size);
    if (vertex_buffer == VK_NULL_HANDLE)
        return NULL;

    VkBuffer index_buffer = VulkanBuffer_CreateStaticIndex(indices, index_count);
    if (index_buffer == VK_NULL_HANDLE)
        return NULL;

    mesh_t *mesh = arena_push(g_engine_arena, mesh_t);
    mesh->vertex_buffer = vertex_buffer;
    mesh->index_buffer = index_buffer;
    mesh->index_count = index_count;

    return mesh;
}

static void insert_predefined_mesh(predefined_mesh_t slot, const void *vertices,
                                   u64 vertices_size, const u32 *indices, u32 index_count)
{
    s_meshes->predefined[slot] = Mesh_Create(vertices, vertices_size, indices, index_count);
}

static void load_predefined_meshes(void)
{
    // Triangles
    {
        static const colored_vertex_t colored_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .color = {{1.0f, 0.0f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .color = {{0.0f, 1.0f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .color = {{0.0f, 0.0f, 1.0f}}},
        };
        static const simple_vertex_t simple_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}},
        };
        static const u32 indices[] = {0, 1, 2};

        insert_predefined_mesh(PREDEFINED_MESH_SIMPLE_TRIANGLE, simple_vertices,
                               sizeof(simple_vertices), indices, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_COLORED_TRIANGLE, colored_vertices,
                               sizeof(colored_vertices), indices, ArrayCount(indices));
    }

    // Quads
    {
        static const simple_vertex_t simple_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}},
        };
        static const normal_vertex_t normaled_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
        };
        static const colored_vertex_t colored_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .color = {{1.0f, 0.0f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .color = {{0.0f, 1.0f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .color = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}, .color = {{1.0f, 0.0f, 1.0f}}},
        };
        static const textured_vertex_t textured_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .texture_coord = {{0.0f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .texture_coord = {{1.0f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .texture_coord = {{0.0f, 1.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}, .texture_coord = {{1.0f, 1.0f}}},
        };
        static const u32 indices[] = {0, 1, 2, 2, 1, 3};

        insert_predefined_mesh(PREDEFINED_MESH_SIMPLE_QUAD, simple_vertices,
                               sizeof(simple_vertices), indices, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_NORMALED_QUAD, normaled_vertices,
                               sizeof(normaled_vertices), indices, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_COLORED_QUAD, colored_vertices,
                               sizeof(colored_vertices), indices, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_TEXTURED_QUAD, textured_vertices,
                               sizeof(textured_vertices), indices, ArrayCount(indices));
    }

    // Cube
    {
        static const normal_vertex_t cube_vertices[] = {
            // +Z
            {.position = {{-0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            {.position = {{ 0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            {.position = {{ 0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            {.position = {{-0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            // -Z
            {.position = {{ 0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            {.position = {{-0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            {.position = {{-0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            {.position = {{ 0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            // +X
            {.position = {{ 0.5f, -0.5f,  0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            {.position = {{ 0.5f, -0.5f, -0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f, -0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f,  0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            // -X
            {.position = {{-0.5f, -0.5f, -0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            {.position = {{-0.5f, -0.5f,  0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            {.position = {{-0.5f,  0.5f,  0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            {.position = {{-0.5f,  0.5f, -0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            // +Y
            {.position = {{-0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            {.position = {{-0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            // -Y
            {.position = {{-0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
            {.position = {{ 0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
            {.position = {{ 0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
            {.position = {{-0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
        };

        static const u32 cube_indices[] = {
            0,  2,  1,  0,  3,  2,  // +Z (clockwise)
            4,  6,  5,  4,  7,  6,  // -Z
            8,  10, 9,  8,  11, 10, // +X
            12, 14, 13, 12, 15, 14, // -X
            16, 18, 17, 16, 19, 18, // +Y
            20, 22, 21, 20, 23, 22, // -Y
        };

        insert_predefined_mesh(PREDEFINED_MESH_NORMALED_CUBE, cube_vertices,
                               sizeof(cube_vertices), cube_indices, ArrayCount(cube_indices));
    }
}
