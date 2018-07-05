cmake_minimum_required(VERSION 3.1)

project(gn-perf LANGUAGES CXX)

# Get code version
execute_process(COMMAND git describe --tags --always
    OUTPUT_VARIABLE GIT_REPO_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "gn-perf ${GIT_REPO_VERSION}")

# Paths
set(INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Load libshadertoy
add_subdirectory(libshadertoy)

# Load dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(Epoxy REQUIRED)
find_package(GLFW3 REQUIRED)

# Configure header
configure_file(${INC_DIR}/gn_perf_config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/gn_perf_config.hpp)

add_executable(gn_perf ${SRC_DIR}/main.cpp)

set_target_properties(gn_perf PROPERTIES CXX_STANDARD 14)
target_include_directories(gn_perf PRIVATE
    ${INC_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(gn_perf PRIVATE
    ${EPOXY_LIBRARIES}
    ${GLFW3_LIBRARIES}
    shadertoy-shared)
