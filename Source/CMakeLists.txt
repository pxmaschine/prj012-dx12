add_subdirectory(ThirdParty)

set(HEADER_FILES
  MathUtility.h
  PrimitiveTypes.h
  Platform/PlatformContext.h
)

set(SOURCE_FILES
  Platform/Win32/Win32Main.cpp
  # Platform/Win32/Win32Window.cpp
  # Platform/DX12/DX12.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
