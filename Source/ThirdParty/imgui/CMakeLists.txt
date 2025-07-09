file(GLOB_RECURSE LIB_SOURCES src/*.cpp)

add_library(imgui STATIC ${LIB_SOURCES})

# Add both include/ and src/ directories to the include path
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
