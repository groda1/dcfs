#include "model.h"
#include "core.h"
#include "core_math.h"
#include "core_string.h"
#include "log.h"
#include "memory_arena.h"

#include "model_internal.h"
#include "mesh.h"
#include "renderer.h"

extern arena_t *g_engine_arena;

/* std430 palette entry; vec3 colors land in vec4 slots so the array stride
   matches the shader */
typedef struct
{
    vec4 base_color;
    vec4 specular_color;
} gpu_material_t;

StaticAssert(sizeof(gpu_material_t) == 32,
             "gpu_material_t must match the shader's std430 material stride");
StaticAssert(offsetof(model_push_constant_t, transform) == 16,
             "model_push_constant_t must match the shader's std430 push constant layout");

struct _model_instance_t
{
    model_handle_t model;
    model_animation_t *idle_animation;

    gpu_material_t *palette;
    buffer_object_handle_t palette_sbo;

    f32 keyframe_time;  // Time elapsed since keyframe start
    model_animation_t *current_animation;
    u16 current_keyframe_idx;

    mesh_handle_t anim_from_mesh;
    mesh_handle_t anim_to_mesh;
    f32           keyframe_progress; // 0.0f -> 1.0f
};


static bool upload_palette(model_instance_t *instance);
static i32 find_material(model_handle_t model, string name);

model_instance_handle_t ModelInstance_New(model_handle_t model)
{
    Assert(g_engine_arena != NULL);
    Assert(model != MODEL_INVALID_HANDLE);
    Assert(model->material_count > 0);

    u64 pos = MemoryArena_Pos(g_engine_arena);

    model_instance_t *instance = arena_push(g_engine_arena, model_instance_t);
    instance->model = model;
    instance->palette = arena_push_array(g_engine_arena, gpu_material_t, model->material_count);

    for (u32 i = 0; i < model->material_count; i++)
    {
        const model_material_t *material = &model->materials[i];
        instance->palette[i].base_color = V4(material->base_color.X, material->base_color.Y,
                                             material->base_color.Z, 1.0f);
        instance->palette[i].specular_color = V4(material->specular_color.X,
                                                 material->specular_color.Y,
                                                 material->specular_color.Z, 1.0f);
    }

    u64 palette_size = sizeof(gpu_material_t) * model->material_count;
    instance->palette_sbo = Renderer_CreateStorageBuffer(palette_size);
    if (instance->palette_sbo == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create material palette storage buffer");
        MemoryArena_PopTo(g_engine_arena, pos);
        return MODEL_INSTANCE_INVALID_HANDLE;
    }

    if (!upload_palette(instance))
    {
        MemoryArena_PopTo(g_engine_arena, pos);
        return MODEL_INSTANCE_INVALID_HANDLE;
    }

    instance->current_animation = MODEL_ANIMATION_INVALID_HANDLE;
    instance->current_keyframe_idx = 0;
    instance->keyframe_time = 0.0f;

    instance->anim_from_mesh = model->animations->keyframes[0].mesh;
    instance->anim_to_mesh = model->animations->keyframes[0].mesh;

    return instance;
}

bool ModelInstance_SetMaterialBaseColor(model_instance_handle_t instance, string material,
                                        vec3 color)
{
    Assert(instance != MODEL_INSTANCE_INVALID_HANDLE);

    i32 slot = find_material(instance->model, material);
    if (slot < 0)
        return false;

    instance->palette[slot].base_color = V4(color.X, color.Y, color.Z, 1.0f);
    return upload_palette(instance);
}

bool ModelInstance_SetMaterialSpecularColor(model_instance_handle_t instance, string material,
                                            vec3 color)
{
    Assert(instance != MODEL_INSTANCE_INVALID_HANDLE);

    i32 slot = find_material(instance->model, material);
    if (slot < 0)
        return false;

    instance->palette[slot].specular_color = V4(color.X, color.Y, color.Z, 1.0f);
    return upload_palette(instance);
}

void ModelInstance_Draw(model_instance_handle_t instance, renderpass_handle_t pass_handle,
                        pipeline_handle_t pipeline, mat4 transform)
{
    Assert(instance != MODEL_INSTANCE_INVALID_HANDLE);

    model_push_constant_t push_constant = {
        .transform = transform,
        .keyframe_t = instance->keyframe_progress,
    };

    mesh_handle_t meshes[2] = {
        instance->anim_from_mesh, instance->anim_to_mesh
    };

    Renderer_DrawMeshes(pass_handle, pipeline, &push_constant, instance->palette_sbo, 1,
                        meshes, ArrayCount(meshes));
}

model_animation_handle_t Model_GetAnimation(model_handle_t model, const char *animation_name)
{
    for (u32 i = 0; i < model->animation_count; i++)
    {
        if (string_match(model->animations[i].name, string_from(animation_name)))
            return &model->animations[i];
    }

    Log(WARNING, "model has no animation named %s", animation_name);
    return MODEL_ANIMATION_INVALID_HANDLE;
}

bool ModelInstance_PlayAnimation(model_instance_handle_t instance, model_animation_handle_t animation)
{
    if (animation->keyframe_count < 2)
        return false;

    instance->current_animation = animation;
    instance->current_keyframe_idx = 0;
    instance->keyframe_time = 0.0f;
    instance->keyframe_progress = 0.0f;

    instance->anim_from_mesh = animation->keyframes[0].mesh;
    instance->anim_to_mesh = animation->keyframes[1].mesh;

    Log(DEBUG, "playing animation %S", animation->name);

    return true;
}

bool ModelInstance_SetIdleAnimation(model_instance_handle_t instance, const char *animation_name)
{
    model_animation_handle_t handle = Model_GetAnimation(instance->model, animation_name);

    if (handle == MODEL_ANIMATION_INVALID_HANDLE)
    {
        Log(WARNING, "model has no animation named %s", animation_name);
        return false;
    }
    instance->idle_animation = handle;
    ModelInstance_PlayAnimation(instance, handle);

    return true;
}

void ModelInstance_Update(model_instance_handle_t instance, f32 delta_time)
{
    model_animation_t *anim = instance->current_animation;
    u16 idx = instance->current_keyframe_idx;

    if (anim == MODEL_ANIMATION_INVALID_HANDLE)
        return;

    instance->keyframe_time += delta_time;

    f32 duration = anim->keyframes[idx + 1].time_s - anim->keyframes[idx].time_s;

    while (instance->keyframe_time >= duration)
    {
        // last segment finished: rest on the final keyframe
        if (idx + 2 >= anim->keyframe_count)
        {
            instance->current_animation = MODEL_ANIMATION_INVALID_HANDLE;
            instance->keyframe_progress = 1.0f;

            if (instance->idle_animation)
                ModelInstance_PlayAnimation(instance, instance->idle_animation);
            return;
        }

        instance->keyframe_time -= duration;
        idx++;
        duration = anim->keyframes[idx + 1].time_s - anim->keyframes[idx].time_s;

        instance->anim_from_mesh = anim->keyframes[idx].mesh;
        instance->anim_to_mesh = anim->keyframes[idx + 1].mesh;
        instance->current_keyframe_idx = idx;
    }

    instance->keyframe_progress = instance->keyframe_time / duration;
}

static bool upload_palette(model_instance_t *instance)
{
    u64 palette_size = sizeof(gpu_material_t) * instance->model->material_count;

    if (!Renderer_SetBufferObject(instance->palette_sbo, instance->palette, palette_size))
    {
        Log(ERROR, "failed to upload material palette");
        return false;
    }
    return true;
}

static i32 find_material(model_handle_t model, string name)
{
    for (u32 i = 0; i < model->material_count; i++)
    {
        if (string_match(model->materials[i].name, name))
            return (i32)i;
    }

    Log(WARNING, "model has no material slot named %S", name);
    return -1;
}
