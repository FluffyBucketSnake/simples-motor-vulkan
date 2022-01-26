#version 450

layout(location = 0) in vec3 fragCor;
layout(location = 1) in vec2 fragCoordTex;

layout(binding = 0, location = 1) uniform sampler2D amostrador;

layout(location = 0) out vec4 saidaCor;

void main() {
    saidaCor = vec4(fragCor, 1.0) * texture(amostrador, fragCoordTex);
}