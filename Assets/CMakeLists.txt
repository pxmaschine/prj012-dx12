
# Copy all other directories except Shaders to the output directory
file(GLOB ASSET_DIRS LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(DIR ${ASSET_DIRS})
    if(IS_DIRECTORY "${DIR}")
        get_filename_component(DIR_NAME ${DIR} NAME)
        add_custom_command(
            OUTPUT "${CMAKE_BINARY_DIR}/${CONFIG_DIR}/Bin/Assets/${DIR_NAME}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${DIR}" "${CMAKE_BINARY_DIR}/${CONFIG_DIR}/Bin/Assets/${DIR_NAME}"
            COMMENT "Copying ${DIR_NAME} to output directory"
        )
        list(APPEND ASSET_COPY_TARGETS "${CMAKE_BINARY_DIR}/${CONFIG_DIR}/Bin/Assets/${DIR_NAME}")
    endif()
endforeach()

add_custom_target(copy_assets ALL DEPENDS ${ASSET_COPY_TARGETS})
