#version 450

layout(location = 0) in vec2 posicao;
layout(location = 1) in vec3 cor;

layout(location = 0) out vec3 fragCor;

void main() { 
    fragCor = cor;
    gl_Position = vec4(posicao, 0.0, 1.0); 
}
