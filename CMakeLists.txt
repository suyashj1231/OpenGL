cmake_minimum_required(VERSION 3.10)
project(OpenGL VERSION 1.0.0)

find_package(OpenGL REQUIRED)

add_executable(OpenGL src/config.h src/main.cpp src/glad.c src/Renderer.cpp src/FontManager.cpp src/Terminal.cpp src/PTYHandler.cpp)

target_include_directories(OpenGL PRIVATE dependencies)

find_package(glfw3 3.3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm REQUIRED)

target_link_libraries(OpenGL glfw OpenGL::GL Freetype::Freetype glm::glm)