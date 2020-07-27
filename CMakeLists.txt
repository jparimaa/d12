cmake_minimum_required(VERSION 3.5)
project(dx12-demos VERSION 0.0.1 LANGUAGES CXX)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "64bit is required")
endif()
if(NOT MSVC)
    message(FATAL_ERROR "Must be MSVC")
endif()

set(GLFW_PATH CACHE PATH "Path to GLFW")
set(ASSIMP_PATH CACHE PATH "Path to assimp")

function(ADD_PROJECT_WITH_DEFAULT_SETTINGS PROJECT_NAME)
    file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

    add_executable(${PROJECT_NAME} ${SOURCES})

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            d12
            d3d12.lib
            dxgi.lib
            dxguid.lib
            dxcompiler.lib
            D3DCompiler.lib
    )

    target_include_directories(${PROJECT_NAME}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/include"
            "${GLFW_PATH}/include"
    )

    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /WX /MP)
    
    add_definitions(-DSHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ASSIMP_PATH}/bin/x64/assimp-vc140-mt.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
endfunction()

add_definitions(-DROOT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/")
add_definitions(-DASSET_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets/")

add_subdirectory(Framework)
add_subdirectory(Examples/Minimal)
add_subdirectory(Examples/Glow)
add_subdirectory(Examples/MotionBlur)
add_subdirectory(Examples/DXR)
add_subdirectory(Examples/DynamicIndexing)