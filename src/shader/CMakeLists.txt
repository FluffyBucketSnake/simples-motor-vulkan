# Buscar arquivos fontes de shader
file(GLOB COMPUTE_SOURCES *.comp)
file(GLOB FRAMENT_SOURCES *.frag)
file(GLOB VERTEX_SOURCES *.vert)
set(SOURCES ${COMPUTE_SOURCES} ${FRAMENT_SOURCES} ${VERTEX_SOURCES})

# Compilar shaders
add_shader_library(shaders ${SOURCES})