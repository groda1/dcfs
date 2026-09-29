#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_buffer_reference : require

struct instance_data {
    mat4 transform;
    uint flags;
    uint pad[3];
};

layout(std430, buffer_reference) readonly buffer InstanceData {
    instance_data instances[];
};

layout(push_constant) uniform pushConstants {
    InstanceData instance_data;
    float transition;
    float pad;
} pc;

layout(location = 0) in vec3 worldPosition;
layout(location = 1) flat in uint instanceIndex;

layout(location = 0) out vec4 outMask;

const uint LIT_BIT = 0u;
const uint HIDDEN_BIT = 1u;
const uint KNOWN_SHIFT = 2u;
const uint KNOWN_FULL = 1u;

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float value_noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float dissolve_noise(vec3 p) {
    return 0.5 * value_noise(p * 2.0)
         + 0.3 * value_noise(p * 5.0)
         + 0.2 * value_noise(p * 11.0);
}

void main() {
    uint flags = pc.instance_data.instances[instanceIndex].flags;

    float lit = (flags & (1u << LIT_BIT)) != 0u ? 1.0 : 0.0;

    float t = clamp(pc.transition, 0.0, 1.0);
    uint known_mode = (flags >> KNOWN_SHIFT) & 3u;
    float known = (known_mode == KNOWN_FULL || t >= 1.0) ? 1.0 : step(dissolve_noise(worldPosition), t);

    if ((flags & (1u << HIDDEN_BIT)) != 0u)
        known = 0.0;

    outMask = vec4(lit, known, 0.0, 1.0);
}
