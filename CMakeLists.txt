cmake_minimum_required(VERSION 3.0.0)
project(mand VERSION 0.1.0)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON )

include(CTest)
enable_testing()


find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
#find_package(opengl3 REQUIRED)
#find_package(SFML REQUIRED network audio) # error
#find_package(glfw REQUIRED FATAL_ERROR) # error

# Add source to this project's executable.
include_directories (
    submodules/imgui/
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
)

#target_link_libraries (MyProject glfw)
#target_link_libraries (MyProject ${SFML_LIBRARIES})

add_executable(mand main.cpp
    submodules/imgui/examples/imgui_impl_sdl.cpp
    submodules/imgui/examples/imgui_impl_opengl3.cpp
    submodules/imgui/imgui_widgets.cpp
    submodules/imgui/imgui_draw.cpp
    submodules/imgui/imgui.cpp
    )
target_link_libraries (mand ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
