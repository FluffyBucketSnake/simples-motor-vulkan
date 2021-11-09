# Buscar os arquivos fontes do binário
file(GLOB SOURCES *.cpp)

# Criar o alvo executável do projeto
add_executable(motor ${SOURCES})

# Ativar avisos de compilação
target_compile_options(motor PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Werror -Wextra -Wconversion -Wsign-conversion -pedantic-errors>
     $<$<CXX_COMPILER_ID:MSVC>:
          /WX /W4>)

# Ligar o alvo com as bibliotecas necessárias
target_link_libraries(motor PRIVATE vulkan)
target_link_libraries(motor PRIVATE glfw)
target_link_libraries(motor PRIVATE stb)