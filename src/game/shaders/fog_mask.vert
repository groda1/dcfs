#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    mat4 transform;
    ivec2 tile;
    uint visibility;
    uint previous;
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
    float tear_depth;
    float edge_softness;
    float transition;
    float pad;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 worldPosition;
layout(location = 1) flat out ivec2 tile;
layout(location = 2) flat out uint visibility;
layout(location = 3) flat out uint previous;

void main() {
    instance_data instance = pc.instance_data.instances[gl_InstanceIndex];

    vec4 world = instance.transform * vec4(inPosition, 1.0);
    gl_Position = vp.proj * vp.view * world;

    worldPosition = world.xyz;
    tile = instance.tile;
    visibility = instance.visibility;
    previous = instance.previous;
}
