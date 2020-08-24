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

# Create symlinks for index.html and style.css file in the build directory.
# (Or copy them if we're on Travis so that deployment works.)
if(DEFINED ENV{TRAVIS})
    file(COPY
        src/index.html
        DESTINATION .)
    file(COPY
        src/style.css
        DESTINATION .)
else()
    file(CREATE_LINK
        ${CMAKE_CURRENT_SOURCE_DIR}/src/index.html
        ${CMAKE_CURRENT_BINARY_DIR}/index.html
        SYMBOLIC)
    file(CREATE_LINK
        ${CMAKE_CURRENT_SOURCE_DIR}/src/style.css
        ${CMAKE_CURRENT_BINARY_DIR}/style.css
        SYMBOLIC)
endif()

# Download the Bootstrap/Tabler CSS/JS files.
file(DOWNLOAD
     https://preview-dev.tabler.io/dist/css/tabler.min.css?1588343458
     ${CMAKE_CURRENT_BINARY_DIR}/tabler.min.css)
file(DOWNLOAD
     https://preview-dev.tabler.io/dist/css/tabler-buttons.min.css?1588343458
     ${CMAKE_CURRENT_BINARY_DIR}/tabler-buttons.min.css)
file(DOWNLOAD
     https://preview-dev.tabler.io/dist/libs/bootstrap/dist/js/bootstrap.bundle.min.js?1588343458
     ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.bundle.min.js)
file(DOWNLOAD
     https://preview-dev.tabler.io/dist/libs/bootstrap/dist/js/tabler.min.js?1588343458
     ${CMAKE_CURRENT_BINARY_DIR}/tabler.min.js)
