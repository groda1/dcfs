#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (push_constant) uniform pushConstants {
    mat4 transform;
    vec4 color;
} model;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in uint inMaterialIdx;

/* View-space quantities handed to the fragment shader, where the actual
 * lighting happens. The face normal is reconstructed per-pixel from the
 * derivatives of vViewPos, so the whole triangle lights as one flat facet
 * regardless of which vertex is provoking. */
layout(location = 0) out vec3 vViewPos;
layout(location = 1) out vec3 vViewNormal;
layout(location = 2) flat out vec3 vLightDirView;

const vec3 light_dir = normalize(vec3(-1.0, -1.0, -1.0));

void main() {
    vec4 worldPos = model.transform * vec4(inPosition, 1.0);
    vec4 viewPos  = vp.view * worldPos;

    gl_Position = vp.proj * viewPos;

    vViewPos      = viewPos.xyz;
    /* Only used to disambiguate the sign of the derived face normal, so the
     * lack of an inverse-transpose here is fine. */
    vViewNormal   = normalize((vp.view * model.transform * vec4(inNormal, 0.0)).xyz);
    /* Constant per draw (view is constant), so flat carries it exactly. */
    vLightDirView = normalize((vp.view * vec4(light_dir, 0.0)).xyz);
}
