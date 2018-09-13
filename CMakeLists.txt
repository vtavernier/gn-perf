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

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Load dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(Boost REQUIRED COMPONENTS filesystem program_options)
find_package(Epoxy REQUIRED)
find_package(GLFW3 REQUIRED)
find_package(PNG REQUIRED)
find_package(NVML)
find_path(PNGPP_INCLUDE_DIR png++/png.hpp)

add_executable(gn_perf ${SRC_DIR}/main.cpp ${SRC_DIR}/gn_glfw.cpp)

set_target_properties(gn_perf PROPERTIES CXX_STANDARD 14)
target_include_directories(gn_perf PRIVATE
    ${INC_DIR}
    ${PNGPP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/PicoSHA2
    ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(gn_perf PRIVATE
    ${EPOXY_LIBRARIES}
    ${GLFW3_LIBRARIES}
    ${Boost_LIBRARIES}
    ${PNG_LIBRARY}
    shadertoy-shared)

if(NVML_FOUND)
    target_include_directories(gn_perf PRIVATE ${NVML_INCLUDE_DIR})
    target_link_libraries(gn_perf PRIVATE ${NVML_LIBRARIES})
    set(HAS_NVML 1)
    message(STATUS "Using NVML for P-state detection")
else()
    set(HAS_NVML 0)
    message(STATUS "Not using NVML")
endif()

# Configure header
configure_file(${INC_DIR}/gn_perf_config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/gn_perf_config.hpp)

target_compile_options(gn_perf PRIVATE -Wall -Wno-attributes)
