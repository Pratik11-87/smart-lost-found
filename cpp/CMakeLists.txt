cmake_minimum_required(VERSION 3.16)
project(smart_lost_found_matcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(slf_matcher matcher.cpp)
target_include_directories(slf_matcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(slf_matcher_demo main.cpp)
target_link_libraries(slf_matcher_demo PRIVATE slf_matcher)
