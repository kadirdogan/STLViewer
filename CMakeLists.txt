cmake_minimum_required(VERSION 3.20)
project(STLViewer LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(STLViewer WIN32
    src/main.cpp
    src/stl.cpp
    src/stl.h
    src/resource.h
    src/app.rc
)
target_compile_definitions(STLViewer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(STLViewer PRIVATE
    comctl32
    comdlg32
    uxtheme
    dwmapi
    opengl32
    glu32
    shell32
)
if(MSVC)
    target_compile_options(STLViewer PRIVATE /W4 /permissive- /utf-8)
endif()
