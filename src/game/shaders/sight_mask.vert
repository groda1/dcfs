#version 450
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

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(push_constant) uniform pushConstants {
    InstanceData instance_data;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 worldPosition;
layout(location = 1) flat out uint instanceIndex;

void main() {
    instance_data instance = pc.instance_data.instances[gl_InstanceIndex];

    vec4 world = instance.transform * vec4(inPosition, 1.0);
    gl_Position = vp.proj * vp.view * world;

    worldPosition = world.xyz;
    instanceIndex = gl_InstanceIndex;
}
