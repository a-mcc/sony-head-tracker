cmake_minimum_required(VERSION 3.20)

project(sony-head-tracker-linux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HIDAPI REQUIRED hidapi-hidraw)

add_executable(sony-head-tracker
        src/main_linux.cpp
        src/hid_backend_linux.cpp
        src/hid_descriptor.cpp
)

target_include_directories(sony-head-tracker PRIVATE
        include
        ${HIDAPI_INCLUDE_DIRS}
)

target_link_libraries(sony-head-tracker PRIVATE
        ${HIDAPI_LIBRARIES}
)