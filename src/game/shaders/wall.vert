#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    mat4 transform;
    vec4 color;
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

layout(location = 0) flat out vec4 fragColor;
layout(location = 1) out vec3 localPosition;
layout(location = 2) flat out vec3 localNormal;

const vec3 light_dir = normalize(vec3(-1.0, -1.0, -0.7));
const vec3 light_color = vec3(1.0, 1.0, 1.0);
const float ambient_strength = 0.15;

void main() {

    instance_data instance = pc.instance_data.instances[gl_InstanceIndex];

    gl_Position = vp.proj * vp.view * instance.transform * vec4(inPosition, 1.0);

    // Lighting
    vec3 normal = normalize((instance.transform * vec4(inNormal, 0.0)).xyz);
    float diffuse_factor = max(dot(normal, -light_dir), 0.0);
    vec3 diffuse = diffuse_factor * light_color;

    localPosition = inPosition;
    localNormal = inNormal;
    fragColor = vec4(min(diffuse + ambient_strength, 1.0), 1.0) * instance.color;
}
