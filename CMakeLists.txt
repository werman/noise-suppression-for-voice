cmake_minimum_required(VERSION 3.6)

include(GNUInstallDirs)

add_subdirectory(src/rnnoise)
add_subdirectory(src/common)
add_subdirectory(src/vst_plugin)
add_subdirectory(src/lv2_plugin)
