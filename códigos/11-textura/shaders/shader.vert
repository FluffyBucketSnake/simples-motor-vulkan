#version 450

layout(location = 0) in vec3 posicao;
layout(location = 1) in vec3 cor;
layout(location = 2) in vec2 coordTex;

layout(binding = 0) uniform OBU {
    mat4 modelo;
    mat4 visao;
    mat4 projecao;
} obu;

layout(location = 0) out vec3 fragCor;
layout(location = 1) out vec2 fragCoordTex;

void main() { 
    fragCor = cor;
    fragCoordTex = coordTex;
    gl_Position = obu.projecao * obu.visao * obu.modelo * vec4(posicao, 1.0);
}
