#version 450

layout(location = 0) in vec3 fragCor;

layout(location = 0) out vec4 saidaCor;

void main() {
    saidaCor = vec4(fragCor, 1.0);
}