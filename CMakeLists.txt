# Copyright 2019 Arthur Sonzogni. All rights reserved.
# Use of this source code is governed by the MIT license that can be found in
# the LICENSE file.

cmake_minimum_required (VERSION 3.11)
project(asm-dom-fun)

# If needed, in release mode, use emscripten optimizations flags.
include(cmake/release-mode.cmake)

# Add the asm-dom library.
include(cmake/asm-dom.cmake)

# Allow the use of Emscripten's Fetch API.
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FETCH=1")
# Enable exceptions.
string(APPEND CMAKE_CXX_FLAGS " -s DISABLE_EXCEPTION_CATCHING=0")

# Add the main target.
add_executable(main src/main.cpp src/color.cpp src/dom.cpp)
set_property(TARGET main PROPERTY CXX_STANDARD 17)
target_link_libraries(main PRIVATE asm-dom)

# # Copy the index.html file into the build directory
# configure_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/src/index.html
#   ${CMAKE_CURRENT_BINARY_DIR}/index.html
# )

# # Copy the style.css file into the build directory
# configure_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/src/style.css
#   ${CMAKE_CURRENT_BINARY_DIR}/style.css
# )
