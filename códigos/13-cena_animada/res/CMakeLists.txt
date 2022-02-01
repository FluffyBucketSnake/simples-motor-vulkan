# Buscar arquivos de recursos
file(GLOB RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*)
list(REMOVE_ITEM RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)

# Criar alvo de recursos
add_custom_target(recursos)

# Criar comandos de cópias
foreach(RESOURCE_FILE ${RESOURCE_FILES})
    get_filename_component(FILENAME ${RESOURCE_FILE} NAME)
    add_custom_command(TARGET recursos PRE_BUILD
                      COMMAND ${CMAKE_COMMAND} -E
                      copy ${RESOURCE_FILE} ${CMAKE_CURRENT_BINARY_DIR})
endforeach(RESOURCE_FILE ${RESOURCE_FILES})