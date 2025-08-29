add_subdirectory(ThirdParty)

set(HEADER_FILES
  Asset.h
  CoreDefs.h
  MathLib.h
  BitFlags.h
  Platform/Input.h
  Platform/Jobs.h
  Platform/Platform.h
  Platform/PlatformContext.h
  Platform/Win32/Win32Platform.h
  Platform/DX12/DX12.h
  Platform/DX12/DX12Utility.h
  Shaders/Shared.h
  Log.h
  Format.h
  Utility.h
  Geometry.h
  Rendering.h
)

set(SOURCE_FILES
  Asset.cpp
  Platform/Jobs.cpp
  Platform/Platform.cpp
  Platform/Win32/Win32Main.cpp
  Platform/Win32/Win32Platform.cpp
  Platform/DX12/DX12.cpp
  Platform/Input.cpp
  Log.cpp
  Geometry.cpp
  Rendering.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(SHADERS
    Shaders/Default.hlsl
    Shaders/cube.hlsl
)

set(SHADER_INCLUDE_FILES
    Shaders/Common.hlsl
    Shaders/Shared.h
)

set(SHADER_OBJECTS)
set(DEFAULT_SHADER_MODEL "5_1")

foreach(FILE ${SHADERS})
    get_filename_component(FILE_WE ${FILE} NAME_WE)
    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/${CONFIG_DIR}/Bin/Shaders")

    # Outputs
    set(OUTPUT_VS_CSO "${OUTPUT_DIR}/${FILE_WE}_vs.cso")
    set(OUTPUT_PS_CSO "${OUTPUT_DIR}/${FILE_WE}_ps.cso")
    set(OUTPUT_VS_PDB "${OUTPUT_DIR}/${FILE_WE}_vs.pdb")
    set(OUTPUT_PS_PDB "${OUTPUT_DIR}/${FILE_WE}_ps.pdb")

    list(APPEND SHADER_OBJECTS "${OUTPUT_VS_CSO}" "${OUTPUT_PS_CSO}")

    add_custom_command(
        OUTPUT "${OUTPUT_VS_CSO}" "${OUTPUT_PS_CSO}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"

        COMMAND fxc.exe
            /nologo
            /EVS
            /Tvs_${DEFAULT_SHADER_MODEL}
            /I "${CMAKE_CURRENT_SOURCE_DIR}"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Od>
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Zi>
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Gfp>
            /Fo "${OUTPUT_VS_CSO}"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Fd"${OUTPUT_VS_PDB}">
            "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"

        COMMAND fxc.exe
            /nologo
            /EPS
            /Tps_${DEFAULT_SHADER_MODEL}
            /I "${CMAKE_CURRENT_SOURCE_DIR}"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Od>
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Zi>
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Gfp>
            /Fo "${OUTPUT_PS_CSO}"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Fd"${OUTPUT_PS_PDB}">
            "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"

        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
        DEPENDS ${SHADER_INCLUDE_FILES}
        COMMENT "Compiling HLSL: ${FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OBJECTS})
add_dependencies(${PROJECT_NAME} shaders)
