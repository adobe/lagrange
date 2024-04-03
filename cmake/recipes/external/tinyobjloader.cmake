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
if(TARGET tinyobjloader::tinyobjloader)
    return()
endif()

message(STATUS "Third-party (external): creating target 'tinyobjloader::tinyobjloader'")

# Tinyobjloader is a big repo for a single header, so we just download the header...
set(TINYOBJLOADER_VERSION "cc327eecf7f8f4139932aec8d75db2d091f412ef")
set(TINYOBJLOADER_URL "https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/${TINYOBJLOADER_VERSION}/tiny_obj_loader.h")

include(CPM)
CPMAddPackage(
    NAME tinyobjloader
    URL ${TINYOBJLOADER_URL}
    URL_HASH MD5=41f22fefe38fcaeda5366323fbe61f65
    DOWNLOAD_NO_EXTRACT 1
    DOWNLOAD_ONLY ON
)

# Generate implementation file
file(WRITE "${tinyobjloader_BINARY_DIR}/tiny_obj_loader.cpp.in" [[
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
]])

configure_file(${tinyobjloader_BINARY_DIR}/tiny_obj_loader.cpp.in ${tinyobjloader_BINARY_DIR}/tiny_obj_loader.cpp COPYONLY)

# Define tinyobjloader library
add_library(tinyobjloader STATIC ${tinyobjloader_BINARY_DIR}/tiny_obj_loader.cpp)
add_library(tinyobjloader::tinyobjloader ALIAS tinyobjloader)
set_target_properties(tinyobjloader PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
target_include_directories(tinyobjloader PUBLIC
    $<BUILD_INTERFACE:${tinyobjloader_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(tinyobjloader PUBLIC TINYOBJLOADER_USE_DOUBLE)

set_target_properties(tinyobjloader PROPERTIES FOLDER third_party)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME tinyobjloader)
install(DIRECTORY ${TINYOBJLOADER_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS tinyobjloader EXPORT Tinyobjloader_Targets)
install(EXPORT Tinyobjloader_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinyobjloader NAMESPACE tinyobjloader::)
