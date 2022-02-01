#version 450

layout(location = 0) in vec3 posicao;
layout(location = 1) in vec3 cor;
layout(location = 2) in vec2 coordTex;

layout(push_constant) uniform Constantes { mat4 modelo; }
constantes;

layout(binding = 0) uniform OBU {
  mat4 visao;
  mat4 projecao;
}
obu;

layout(location = 0) out vec3 fragCor;
layout(location = 1) out vec2 fragCoordTex;

void main() {
  fragCor = cor;
  fragCoordTex = coordTex;
  gl_Position =
      obu.projecao * obu.visao * constantes.modelo * vec4(posicao, 1.0);
}
