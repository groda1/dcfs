#ifndef MODEL_H
#define MODEL_H

#include "core.h"
#include "core_math.h"
#include "core_string.h"

#include "mesh.h"
#include "render_types.h"

#define MODEL_INVALID_HANDLE NULL
#define MODEL_INSTANCE_INVALID_HANDLE NULL
#define MODEL_ANIMATION_INVALID_HANDLE NULL

typedef struct _model_t             model_t;
typedef struct _model_material_t    model_material_t;
typedef struct _model_keyframe_t    model_keyframe_t;
typedef struct _model_anchor_t      model_anchor_t;
typedef struct _model_animation_t   model_animation_t;

typedef const model_t *model_handle_t;

typedef struct _model_instance_t model_instance_t;
typedef model_instance_t *model_instance_handle_t;
typedef model_animation_t *model_animation_handle_t;

typedef struct
{
    sbo_push_constant_t palette;
    mat4 transform;
    f32 keyframe_t;
} model_push_constant_t;


model_instance_handle_t ModelInstance_New(model_handle_t model);

bool ModelInstance_SetMaterialBaseColor(model_instance_handle_t instance, string material,
                                        vec3 color);
bool ModelInstance_SetMaterialSpecularColor(model_instance_handle_t instance, string material,
                                            vec3 color);

void ModelInstance_Draw(model_instance_handle_t instance, renderpass_handle_t pass_handle,
                        pipeline_handle_t pipeline, mat4 transform);



bool ModelInstance_PlayAnimation(model_instance_handle_t instance, model_animation_handle_t animation);

void ModelInstance_Update(model_instance_handle_t instance, f32 delta_time);

bool ModelInstance_SetIdleAnimation(model_instance_handle_t instance, const char *animation_name);

model_animation_handle_t Model_GetAnimation(model_handle_t model, const char *animation_name);

#endif
