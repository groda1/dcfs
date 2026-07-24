#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (push_constant) uniform pushConstants {
    mat4 transform;
    vec4 color;
} model;

layout(location = 0) in vec3 vViewPos;
layout(location = 1) in vec3 vViewNormal;
layout(location = 2) flat in vec3 vLightDirView;

layout(location = 0) out vec4 outColor;

const vec3 light_color = vec3(1.0, 1.0, 1.0);
const float ambient_strength = 0.0;

void main() {
    /* True geometric normal of this triangle, reconstructed from the
     * view-space position gradient. Constant across the triangle -> a clean,
     * uniform facet that does not depend on the provoking vertex. */
    vec3 N = normalize(cross(dFdx(vViewPos), dFdy(vViewPos)));
    /* The derivative sign depends on screen-space winding; flip N to point
     * the same way as the authored outward normal. */
    if (dot(N, vViewNormal) < 0.0) {
        N = -N;
    }

    float diffuse_factor = max(dot(N, -vLightDirView), 0.0);
    vec3 diffuse = diffuse_factor * light_color;

    vec3 eyeVector = -normalize(vViewPos);
    vec3 reflectVector = normalize(reflect(vLightDirView, N));

    float spec_factor = pow(max(dot(eyeVector, reflectVector), 0.0), 64);
    vec3 specular = 0.15 * spec_factor * light_color;

    vec3 rgb = min(diffuse + ambient_strength, 1.0) * model.color.xyz + specular;
    outColor = vec4(rgb, 1.0);
}
