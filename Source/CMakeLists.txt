add_subdirectory(ThirdParty)

set(HEADER_FILES
  CoreTypes.h
  MathUtility.h
  LoadImage.h
  BitFlags.h
  Platform/PlatformContext.h
  Platform/DX12/DX12.h
  Platform/DX12/DX12Utility.h
  Shaders/Shared.h
)

set(SOURCE_FILES
  Platform/Win32/Win32Main.cpp
  # Platform/Win32/FuckYou.cpp
  # Platform/Win32/Win32Window.cpp
  Platform/DX12/DX12.cpp
  LoadImage.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(PIXEL_SHADERS
    Shaders/cube_ps.hlsl
)

set(VERTEX_SHADERS
    Shaders/cube_vs.hlsl
)

set(SHADER_INCLUDE_FILES
    Shaders/Common.hlsl
    Shaders/Shared.h
)

set(HLSL_SHADER_FILES ${VERTEX_SHADERS} ${PIXEL_SHADERS})

set_source_files_properties(${VERTEX_SHADERS} PROPERTIES ShaderType "vs")
set_source_files_properties(${PIXEL_SHADERS} PROPERTIES ShaderType "ps")
set_source_files_properties(${HLSL_SHADER_FILES} PROPERTIES ShaderModel "5_1")

set(SHADER_OBJECTS)

foreach(FILE ${HLSL_SHADER_FILES})
    get_filename_component(FILE_WE ${FILE} NAME_WE)
    get_filename_component(FILE_PATH ${FILE} DIRECTORY)
    get_source_file_property(shadertype ${FILE} ShaderType)
    get_source_file_property(shadermodel ${FILE} ShaderModel)

    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/${CONFIG_DIR}/Bin/Shaders")
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
            /I "${CMAKE_CURRENT_SOURCE_DIR}"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Od>
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Zi>
            /Fo "${OUTPUT_CSO}"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Fd"${OUTPUT_PDB}">
            "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
        DEPENDS ${SHADER_INCLUDE_FILES}
        COMMENT "Compiling HLSL: ${FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OBJECTS})
add_dependencies(${PROJECT_NAME} shaders)
