cmake_minimum_required(VERSION 3.5)
project(myvk-fw VERSION 1.0.0 LANGUAGES CXX)

set(LIBRARY_NAME d12)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/fw/*.h)
add_library(${LIBRARY_NAME} ${SOURCES})

target_link_libraries(${LIBRARY_NAME}
    PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
        D3DCompiler.lib
        "${GLFW_PATH}/lib-vc2015/glfw3.lib"
)

target_include_directories(${LIBRARY_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/fw"
        "${GLFW_PATH}/include"
)

target_compile_features(${LIBRARY_NAME} 
    PUBLIC 
        cxx_std_17
)

target_compile_options(${LIBRARY_NAME} PRIVATE /W3 /WX /MP)