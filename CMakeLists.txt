cmake_minimum_required(VERSION 3.20)

project(VulkanLesson04 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(vulkan_lesson_04
    src/main.cpp
)

target_link_libraries(vulkan_lesson_04
    PRIVATE
        Vulkan::Vulkan
        glfw
)
