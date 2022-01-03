#version 450

layout(location = 0) in vec2 posicao;
layout(location = 1) in vec3 cor;

layout(binding = 0) uniform OBU {
    mat4 modelo;
    mat4 visao;
    mat4 projecao;
} obu;

layout(location = 0) out vec3 fragCor;

void main() { 
    fragCor = cor;
    gl_Position = obu.projecao * obu.visao * obu.modelo * vec4(posicao, 1.0);
}
