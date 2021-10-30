find_package(Vulkan COMPONENTS glslc)
find_program(GLSLC_EXECUTABLE glslc HINTS Vulkan::glslc)

function(add_shader_library TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "GLSL;HLSL" "ENV;FORMAT" "SOURCES")
    foreach(SHADER ${ARG_SOURCES})
        get_filename_component(FILENAME ${SHADER} NAME)
        set(OUTPUT_SHADER ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.spv)
        set(COMPILED_SHADERS ${COMPILED_SHADERS} ${OUTPUT_SHADER})
        set(COMPILED_SHADERS ${COMPILED_SHADERS} PARENT_SCOPE)
        add_custom_command(
            OUTPUT ${OUTPUT_SHADER}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader '${OUTPUT_SHADER}'"
            COMMAND
                ${GLSLC_EXECUTABLE}
                $<$<BOOL:${ARG_ENV}>:--target-env=${ARG_ENV}>
                $<$<BOOL:${ARG_FORMAT}>:-mfmt=${ARG_FORMAT}>
                $<IF:$<BOOL:${ARG_GLSL}>,-xglsl,$<$<BOOL:${ARG_HLSL}>:-xhlsl>>
                -o ${OUTPUT_SHADER}
                ${SHADER}
        )
    endforeach()
    add_custom_target(${TARGET} ALL DEPENDS ${COMPILED_SHADERS})
endfunction()