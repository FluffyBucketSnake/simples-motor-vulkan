# Buscar os arquivos fontes do binário
file(GLOB SOURCES *.cpp)

# Criar o alvo executável do projeto
add_executable(motor ${SOURCES})

# Ligar o alvo com as bibliotecas necessárias
target_link_libraries(motor PRIVATE vulkan)
target_link_libraries(motor PRIVATE glfw)

# Adicionar shaders
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/shader shader)

add_dependencies(motor shaders)