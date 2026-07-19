cmake_minimum_required(VERSION 3.20)

project(VulkanLesson15 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_program(GLSLC glslc REQUIRED)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/triangle.vert.spv"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLC}
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
            -o "${SHADER_OUTPUT_DIR}/triangle.vert.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/triangle.frag.spv"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLC}
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
            -o "${SHADER_OUTPUT_DIR}/triangle.frag.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
    VERBATIM
)

add_custom_target(vulkan_lesson_15_shaders
    DEPENDS
        "${SHADER_OUTPUT_DIR}/triangle.vert.spv"
        "${SHADER_OUTPUT_DIR}/triangle.frag.spv"
)

add_executable(vulkan_lesson_15
    src/main.cpp
    shaders/triangle.vert
    shaders/triangle.frag
)

set_source_files_properties(
    shaders/triangle.vert
    shaders/triangle.frag
    PROPERTIES HEADER_FILE_ONLY TRUE
)

source_group("Shaders" FILES
    shaders/triangle.vert
    shaders/triangle.frag
)

add_dependencies(vulkan_lesson_15 vulkan_lesson_15_shaders)

target_compile_definitions(vulkan_lesson_15
    PRIVATE
        SHADER_DIR="${SHADER_OUTPUT_DIR}"
)

target_link_libraries(vulkan_lesson_15
    PRIVATE
        Vulkan::Vulkan
        glfw
)
