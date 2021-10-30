# Buscar arquivos fontes de shader
file(GLOB COMPUTE_SOURCES *.comp)   # Shaders de computação
file(GLOB VERTEX_SOURCES *.vert)    # Shaders de vértices
file(GLOB FRAMENT_SOURCES *.frag)   # Shaders de fragmentos
set(SOURCES ${COMPUTE_SOURCES} ${VERTEX_SOURCES} ${FRAMENT_SOURCES})

# Compilar shaders
add_shader_library(shaders GLSL SOURCES ${SOURCES})