# Versão mínima necessária do CMake
cmake_minimum_required(VERSION 3.16)

# Definir o nome deste projeto como um todo
project(simples-motor-vulkan)

# Adicionar pasta cmake/modules na lista de pastas de módulos
LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# Incluir módulos
include(AddShaderLibrary)

# Impor que o compilador use o padrão C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar dependências
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/src bin)