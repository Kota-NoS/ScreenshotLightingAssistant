cmake_minimum_required(VERSION 3.23)
project(SparkleNifBuilder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED NIFLY_ROOT)
    message(FATAL_ERROR "Pass -DNIFLY_ROOT=<path to nifly>")
endif()

add_subdirectory("${NIFLY_ROOT}" "${CMAKE_BINARY_DIR}/nifly")
add_executable(particle_field_nif_builder create_sparkle_nif.cpp)
target_link_libraries(particle_field_nif_builder PRIVATE nifly)
target_include_directories(particle_field_nif_builder PRIVATE "${NIFLY_ROOT}/include")

add_executable(inspect_nif inspect_nif.cpp)
target_link_libraries(inspect_nif PRIVATE nifly)
target_include_directories(inspect_nif PRIVATE "${NIFLY_ROOT}/include")
