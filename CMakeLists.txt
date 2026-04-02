cmake_minimum_required(VERSION 3.31)
project(SolarSystemSim)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find installed packages
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(include)  # Your headers
include_directories(/opt/homebrew/include)  # Homebrew libs
include_directories(lib/imgui)
include_directories(lib/imgui/backends)

# Link directories (fix linking issues)
link_directories(/opt/homebrew/lib)

add_executable(SolarSystemSim main.cpp
        src/CelestialBody.cpp
        src/CelestialBody.h
        src/Camera.cpp
        src/Camera.h
        src/Physics.cpp
        src/Physics.h
        src/FabricGrid.cpp
        src/FabricGrid.h
        lib/imgui/imgui.cpp
        lib/imgui/imgui_draw.cpp
        lib/imgui/imgui_tables.cpp
        lib/imgui/imgui_widgets.cpp
        lib/imgui/backends/imgui_impl_glfw.cpp
        lib/imgui/backends/imgui_impl_opengl3.cpp)

# Link libraries
target_link_libraries(SolarSystemSim glfw glm::glm OpenGL::GL)