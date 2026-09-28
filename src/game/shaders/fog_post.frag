#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in uint colorTexture;
layout(location = 2) flat in uint maskTexture;
layout(location = 3) flat in vec2 fogParams;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(textures[nonuniformEXT(colorTexture)], fragTexCoord).rgb;
    vec2 mask = texture(textures[nonuniformEXT(maskTexture)], fragTexCoord).rg;
    float lit = mask.r;
    float known = mask.g;

    float luminance = dot(color, vec3(0.30, 0.59, 0.11));
    vec3 fogged = mix(color, vec3(luminance), fogParams.y) * fogParams.x;

    outColor = vec4(mix(fogged, color, lit) * known, 1.0);
}
