cmake_minimum_required(VERSION 3.1)
project(tetgen_gui)

include_directories(${CMAKE_SOURCE_DIR}/external)
include_directories(${CMAKE_SOURCE_DIR}/external/tetgen)
include_directories(${CMAKE_SOURCE_DIR}/external/fmt/include)

add_executable(${PROJECT_NAME} tetgen_gui.cpp)

# tetgen lib
file(GLOB tetgen_src ${CMAKE_SOURCE_DIR}/external/tetgen/*.cxx)
add_library(tetgen STATIC ${tetgen_src})
target_compile_definitions(tetgen PRIVATE -DTETLIBRARY)
target_link_libraries(${PROJECT_NAME} tetgen)

# imgui lib
add_subdirectory(${CMAKE_SOURCE_DIR}/external/imgui)
target_link_libraries(${PROJECT_NAME} imgui)

# glfw and glad lib
add_subdirectory(${CMAKE_SOURCE_DIR}/external/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

file(GLOB glad_src ${CMAKE_SOURCE_DIR}/external/glad/*.c)
add_library(glad STATIC ${glad_src})
target_link_libraries(${PROJECT_NAME} glad)

# fmt lib
add_subdirectory(${CMAKE_SOURCE_DIR}/external/fmt)
target_link_libraries(${PROJECT_NAME} fmt)
