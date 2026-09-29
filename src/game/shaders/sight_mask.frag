#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_buffer_reference : require

struct instance_data {
    mat4 transform;
    uint flags;
    float lit;
    float reveal;
    uint pad;
};

layout(std430, buffer_reference) readonly buffer InstanceData {
    instance_data instances[];
};

layout(push_constant) uniform pushConstants {
    InstanceData instance_data;
} pc;

layout(location = 0) in vec3 worldPosition;
layout(location = 1) flat in uint instanceIndex;

layout(location = 0) out vec4 outMask;

const uint HIDDEN_BIT = 0u;
const float REVEAL_GLOW_WIDTH = 0.12;

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
    return 0.75 * value_noise(p * 2.0)
         + 0.25 * value_noise(p * 5.0);
}

void main() {
    instance_data inst = pc.instance_data.instances[instanceIndex];

    float front = inst.reveal * (1.0 + REVEAL_GLOW_WIDTH);
    float behind = front - dissolve_noise(worldPosition);
    float known = inst.reveal >= 1.0 ? 1.0 : step(0.0, behind);
    float glow = inst.reveal >= 1.0 ? 0.0 : known * (1.0 - clamp(behind / REVEAL_GLOW_WIDTH, 0.0, 1.0));

    if ((inst.flags & (1u << HIDDEN_BIT)) != 0u)
    {
        known = 0.0;
        glow = 0.0;
    }

    outMask = vec4(inst.lit, known, glow, 1.0);
}
