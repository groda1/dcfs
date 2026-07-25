#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_buffer_reference : require

struct material_t {
    vec4 base_color;
    vec4 specular_color;
};

layout(std430, buffer_reference) readonly buffer MaterialPalette {
    material_t materials[];
};

layout (push_constant) uniform pushConstants {
    MaterialPalette palette;
    mat4 transform;
    float keyframe_t;
} pc;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} vp;

layout(location = 0) in vec3 inPosition_1;
layout(location = 1) in vec3 inNormal_1;
layout(location = 2) in uint inMaterialIdx_1;

layout(location = 3) in vec3 inPosition_2;
layout(location = 4) in vec3 inNormal_2;
layout(location = 5) in uint inMaterialIdx_2;

layout(location = 0) flat out vec3 fragColor;

const vec3 light_dir = normalize(vec3(-1.0, -1.0, -1.0));
const vec3 light_color = vec3(1.0, 1.0, 1.0);

const float ambient_strength = 0.05;

void main() {
    material_t material = pc.palette.materials[inMaterialIdx_1];

    vec3 inPosition = mix(inPosition_1, inPosition_2, pc.keyframe_t);
    vec3 inNormal = mix(inNormal_1, inNormal_2, pc.keyframe_t);

    vec3 position = (pc.transform * vec4(inPosition, 1.0)).xyz;
    vec3 normal = normalize((pc.transform * vec4(inNormal, 0.0)).xyz);

    gl_Position = vp.proj * vp.view * vec4(position, 1.0);

    float diffuse_factor = max(dot(normal, -light_dir), 0.0);
    vec3 diffuse = diffuse_factor * light_color;

    vec3 eyeVector = -normalize((vp.view * vec4(position, 1.0)).xyz);
    vec3 reflectVectorWorld = normalize(reflect(light_dir, normal));
    vec3 reflectVector = normalize(vec3(vp.view * vec4(reflectVectorWorld, 0.0)));

    float spec_factor = pow(max(dot(eyeVector, reflectVector), 0.0), 64);
    vec3 specular = spec_factor * material.specular_color.rgb;

    fragColor = min(diffuse + ambient_strength, 1.0) * material.base_color.rgb + specular;
}
