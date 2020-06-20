ADD_PROJECT_WITH_DEFAULT_SETTINGS(DXR)

add_custom_command(TARGET DXR POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/dxcompiler.dll"
    $<TARGET_FILE_DIR:DXR>
)

add_custom_command(TARGET DXR POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
    "${CMAKE_BINARY_DIR}/Examples/DXR/"
)