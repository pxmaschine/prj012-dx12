add_library(d3d12headers INTERFACE)

target_include_directories(d3d12headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
