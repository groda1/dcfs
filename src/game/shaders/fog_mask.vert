#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    mat4 transform;
    uint boxes;
    uint flags;
    float surface[9];
    float edge[16];
    float known_surface[9];
    float known_edge[16];
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
layout(location = 1) flat out uint instanceIndex;
layout(location = 2) out vec2 local;

void main() {
    instance_data instance = pc.instance_data.instances[gl_InstanceIndex];

    vec4 world = instance.transform * vec4(inPosition, 1.0);
    gl_Position = vp.proj * vp.view * world;

    worldPosition = world.xyz;
    instanceIndex = gl_InstanceIndex;
    local = inPosition.xy + 0.5;
}
