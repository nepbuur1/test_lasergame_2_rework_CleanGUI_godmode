# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Windows limits a command line to 32767 characters. With the long project
# path and the many include dirs in main/CMakeLists.txt, the compile command
# for main.cpp exceeds that ("ninja: fatal: CreateProcess: The parameter is
# incorrect"). This makes Ninja pass the flags via response files instead.
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(app-template)
