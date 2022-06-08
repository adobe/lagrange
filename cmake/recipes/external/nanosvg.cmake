#
# Copyright 2020 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET nanosvg::nanosvg)
    return()
endif()

message(STATUS "Third-party (external): creating target 'nanosvg::nanosvg'")

include(FetchContent)
FetchContent_Declare(
    nanosvg
    GIT_REPOSITORY https://github.com/memononen/nanosvg.git
    GIT_TAG 25241c5a8f8451d41ab1b02ab2d865b01600d949
)
FetchContent_MakeAvailable(nanosvg)

# Generate implementation file
file(WRITE "${nanosvg_BINARY_DIR}/nanosvg.cpp.in" [[
    #include <math.h>
    #include <stdio.h>
    #include <string.h>

    #define NANOSVG_IMPLEMENTATION
    #include <nanosvg.h>
]])

configure_file(${nanosvg_BINARY_DIR}/nanosvg.cpp.in ${nanosvg_BINARY_DIR}/nanosvg.cpp COPYONLY)

# Define nanosvg library
add_library(nanosvg ${nanosvg_BINARY_DIR}/nanosvg.cpp)
add_library(nanosvg::nanosvg ALIAS nanosvg)

include(GNUInstallDirs)
target_include_directories(nanosvg SYSTEM PUBLIC
    $<BUILD_INTERFACE:${nanosvg_SOURCE_DIR}/src/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(nanosvg PROPERTIES FOLDER "third_party")

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME nanosvg)
install(FILES ${nanosvg_SOURCE_DIR}/src/nanosvg.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS nanosvg EXPORT Nanosvg_Targets)
install(EXPORT Nanosvg_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nanosvg NAMESPACE nanosvg::)
