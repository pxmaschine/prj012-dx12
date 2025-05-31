add_subdirectory(ThirdParty)

add_executable(${PROJECT_NAME} WIN32)

target_sources(${PROJECT_NAME}
  PRIVATE
    Win32Main.cpp
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
