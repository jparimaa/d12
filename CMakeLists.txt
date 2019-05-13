cmake_minimum_required(VERSION 3.5)
project(dx12-demos VERSION 0.0.1 LANGUAGES CXX)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "64bit is required")
endif()

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

set(PROJECT_NAME Minimal)
add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /WX /MP)
else()
    message(FATAL_ERROR "Must be MSVC")
endif()

target_link_libraries(${PROJECT_NAME}
    d3d12.lib
    dxgi.lib
    dxguid.lib
)