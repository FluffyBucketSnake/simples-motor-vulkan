# Buscar os arquivos fontes do binário
file(GLOB SOURCES *.cpp)

# Criar o alvo executável do projeto
add_executable(motor ${SOURCES})

# Ligar o alvo com as bibliotecas necessárias
target_link_libraries(motor PRIVATE vulkan)
target_link_libraries(motor PRIVATE glfw)