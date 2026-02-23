cmake_minimum_required(VERSION 3.30)
message("Using toolchain file ${CMAKE_TOOLCHAIN_FILE}.")

project(
  SeasonalWeatherSKSE
  VERSION 1.0.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)

set(ENABLE_SKYRIM_VR OFF)

include(GNUInstallDirs)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY
)

set(PUBLIC_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME})

# Add headers (from src/ and includes/)
file(
  GLOB_RECURSE
  headers
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/includes/*.h
)

# Add sources
file(
  GLOB_RECURSE
  sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

source_group(
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES ${headers} ${sources}
)

set(BUILD_TESTS OFF)
add_subdirectory(extern/commonlibsse-ng)
include(extern/commonlibsse-ng/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(
  ${PROJECT_NAME}
  AUTHOR colinswrath
  SOURCES ${headers} ${sources}
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

find_package(spdlog CONFIG REQUIRED)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
  spdlog::spdlog
)

target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE
  UNICODE
  _UNICODE
)

target_include_directories(
  ${PROJECT_NAME}
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
  $<INSTALL_INTERFACE:src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/includes>
)

target_precompile_headers(
  ${PROJECT_NAME}
  PRIVATE
  src/pch.h
)

install(
  DIRECTORY ${PUBLIC_HEADER_DIR}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  TARGETS ${PROJECT_NAME}
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(BUILD_NAME Debug)
else()
  set(BUILD_NAME Release)
endif()

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND
  ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Plugin${BUILD_NAME}/skse/plugins
  COMMAND
  ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Plugin${BUILD_NAME}/skse/plugins
  COMMAND
  ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Plugin${BUILD_NAME}/skse/plugins
)

file(GLOB_RECURSE OUTPUT_DLLS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/**/*.dll)
file(GLOB_RECURSE OUTPUT_PDBS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/**/*.pdb)

set_property(
  TARGET ${PROJECT_NAME}
  APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${OUTPUT_DLLS} ${OUTPUT_PDBS}
)
