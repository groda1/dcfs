#include "core.h"
#include "core_math.h"
#include "core_string.h"

#include "model.h"

struct _model_material_t
{
    string name;
    vec3 base_color;
    vec3 specular_color;
};

struct _model_anchor_t
{
    vec3 pos;
    quat orientation;
};

struct _model_keyframe_t {
    f32 time_s;
    mesh_handle_t mesh;
    model_anchor_t *anchors;
};

struct _model_animation_t
{
    string name;
    u16 keyframe_count;

    model_keyframe_t *keyframes;
};

struct _model_t
{
    u16 material_count;
    u16 anchor_count;
    u16 animation_count;

    model_material_t *materials;
    string *anchor_names;
    model_animation_t *animations;
};
