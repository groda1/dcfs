#version 450
#extension GL_EXT_buffer_reference : require

struct instance_data {
    vec2 position;
    vec2 size;
    uint color_texture;
    uint mask_texture;
    float fog_brightness;
    float fog_desaturation;
};

layout(std430, buffer_reference) readonly buffer InstanceData {
    instance_data instances[];
};

layout(push_constant) uniform pushConstants {
    InstanceData instance_data;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) flat out uint colorTexture;
layout(location = 2) flat out uint maskTexture;
layout(location = 3) flat out vec2 fogParams;

void main() {
    instance_data instance = pc.instance_data.instances[gl_InstanceIndex];

    gl_Position = vec4(inPosition.xy * instance.size + instance.position, 0.0, 1.0);

    fragTexCoord = inTexCoord;
    colorTexture = instance.color_texture;
    maskTexture = instance.mask_texture;
    fogParams = vec2(instance.fog_brightness, instance.fog_desaturation);
}
