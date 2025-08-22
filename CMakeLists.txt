cmake_minimum_required(VERSION 3.20)
project(prj012_dx12)

set(EXECUTABLE_NAME "Game")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

##########################################################################################
# Project executable setup
##########################################################################################

# Detect current config from CMAKE_CFG_INTDIR (multi-config generators like Visual Studio or Ninja Multi-Config)
if(CMAKE_CONFIGURATION_TYPES)
    set(CONFIG_DIR "$<CONFIG>")
else()
    set(CONFIG_DIR "${CMAKE_BUILD_TYPE}")
endif()

add_subdirectory(Assets)
add_subdirectory(Source)

# target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set_target_properties(${PROJECT_NAME} PROPERTIES
  OUTPUT_NAME ${EXECUTABLE_NAME}

  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Lib
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin
  
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/Lib
  LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/Bin
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/Bin

  ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/Lib
  LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/Bin
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/Bin

  ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel/Lib
  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel/Bin
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel/Bin

  ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo/Lib
  LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo/Bin
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo/Bin
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
if (MSVC)
  target_compile_definitions(${PROJECT_NAME} PUBLIC 
    -DNOMINMAX
    -WIN32_LEAN_AND_MEAN
  )
  target_compile_options(${PROJECT_NAME} PUBLIC 
    /W4 
    /WX 
    /GR-
    /EHsc /D_HAS_EXCEPTIONS=0
    $<$<CONFIG:Debug>:/Zi /Ob0 /Od /RTC1>
    $<$<CONFIG:Release>:/O2 /DNDEBUG>
  )
else()
  # TODO
endif()

##########################################################################################
# Linking out libraries
##########################################################################################
target_link_libraries(${PROJECT_NAME} PRIVATE
  d3d12headers
  stb
  simplemath
  fmt
  imgui
  cgltf
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  d3d12.lib
  dxgi.lib
  dxguid.lib
  D3DCompiler.lib
  Shlwapi.lib
)

# target_link_libraries(${PROJECT_NAME} PRIVATE
#   Shlwapi.lib
# )

##########################################################################################
# Post-Build Commands
##########################################################################################
