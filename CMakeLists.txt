cmake_minimum_required(VERSION 3.16)
project(smart_lost_found LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.3.tar.gz
)
FetchContent_MakeAvailable(httplib)

add_executable(smart-lost-found cpp-web/server.cpp cpp/matcher.cpp)
target_include_directories(smart-lost-found PRIVATE cpp)
target_link_libraries(smart-lost-found PRIVATE httplib::httplib)
if(WIN32)
  target_link_libraries(smart-lost-found PRIVATE ws2_32)
endif()
