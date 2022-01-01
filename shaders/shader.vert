#version 450

layout(location = 0) in vec2 posicao;

layout(location = 0) out vec3 fragCor;

void main() { 
    fragCor = vec3(1.0, 1.0, 1.0);
    gl_Position = vec4(posicao, 0.0, 1.0); 
}
