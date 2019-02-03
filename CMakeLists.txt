cmake_minimum_required(VERSION 3.6)

include(GNUInstallDirs)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BINARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src/rnnoise)
add_subdirectory(src/common)
add_subdirectory(src/vst_plugin)
add_subdirectory(src/lv2_plugin)
add_subdirectory(src/ladspa_plugin)