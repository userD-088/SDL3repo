cmake_minimum_required(VERSION 3.20)

project(SDL3repo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

add_executable(SDL3repo
    src/main.cpp
    src/Launcher.cpp
    src/GameInfo.cpp
    src/Utils.cpp
    src/Pong.cpp
)

target_include_directories(SDL3repo PRIVATE
    include
)

target_link_libraries(SDL3repo PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
)
