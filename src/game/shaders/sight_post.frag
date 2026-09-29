#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in uint colorTexture;
layout(location = 2) flat in uint maskTexture;
layout(location = 3) flat in vec2 rememberedLook;

layout(location = 0) out vec4 outColor;

const vec3 REVEAL_GLOW_COLOR = vec3(1.0, 0.75, 0.45) * 0.6;

void main() {
    vec3 color = texture(textures[nonuniformEXT(colorTexture)], fragTexCoord).rgb;
    vec3 mask = texture(textures[nonuniformEXT(maskTexture)], fragTexCoord).rgb;
    float lit = mask.r;
    float known = mask.g;
    float glow = mask.b;

    float luminance = dot(color, vec3(0.30, 0.59, 0.11));
    vec3 remembered = mix(color, vec3(luminance), rememberedLook.y) * rememberedLook.x;

    outColor = vec4(mix(remembered, color, lit) * known + REVEAL_GLOW_COLOR * glow, 1.0);
}
