set(PIXEL_SHADERS
    Shaders/cube_ps.hlsl
)

set(VERTEX_SHADERS
    Shaders/cube_vs.hlsl
)

set(HLSL_SHADER_FILES ${VERTEX_SHADERS} ${PIXEL_SHADERS})

set_source_files_properties(${VERTEX_SHADERS} PROPERTIES ShaderType "vs")
set_source_files_properties(${PIXEL_SHADERS} PROPERTIES ShaderType "ps")
set_source_files_properties(${HLSL_SHADER_FILES} PROPERTIES ShaderModel "5_1")

set(SHADER_OBJECTS)

# Detect current config from CMAKE_CFG_INTDIR (multi-config generators like Visual Studio or Ninja Multi-Config)
if(CMAKE_CONFIGURATION_TYPES)
    set(CONFIG_DIR "$<CONFIG>")
else()
    set(CONFIG_DIR "${CMAKE_BUILD_TYPE}")
endif()

foreach(FILE ${HLSL_SHADER_FILES})
    get_filename_component(FILE_WE ${FILE} NAME_WE)
    get_filename_component(FILE_PATH ${FILE} DIRECTORY)
    get_source_file_property(shadertype ${FILE} ShaderType)
    get_source_file_property(shadermodel ${FILE} ShaderModel)

    # Use output directory path with CONFIG_DIR
    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/${CONFIG_DIR}/Bin/Assets/Shaders")
    set(OUTPUT_CSO "${OUTPUT_DIR}/${FILE_WE}.cso")
    set(OUTPUT_PDB "${OUTPUT_DIR}/${FILE_WE}.pdb")

    list(APPEND SHADER_OBJECTS "${OUTPUT_CSO}")

    add_custom_command(
        OUTPUT "${OUTPUT_CSO}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
        COMMAND fxc.exe
            /nologo
            /Emain
            /T${shadertype}_${shadermodel}
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Od>
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Zi>
            /Fo "${OUTPUT_CSO}"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Fd"${OUTPUT_PDB}">
            "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
        COMMENT "Compiling HLSL: ${FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OBJECTS})
