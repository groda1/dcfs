#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) flat in vec3 fragColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outMask;

void main() {
    outColor = vec4(fragColor, 1.0);
    outMask = vec4(1.0, 1.0, 0.0, 1.0);
}
