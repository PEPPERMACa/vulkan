#version 450

layout(location = 0) in vec3 fragmentColor;
layout(location = 1) in vec2 fragmentTextureCoordinate;
layout(location = 0) out vec4 outputColor;

layout(binding = 1) uniform sampler2D textureSampler;

void main() {
    outputColor = texture(textureSampler, fragmentTextureCoordinate) *
        vec4(fragmentColor, 1.0);
}
