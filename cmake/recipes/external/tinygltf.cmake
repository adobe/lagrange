#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET tinygltf::tinygltf)
    return()
endif()

message(STATUS "Third-party (external): creating target 'tinygltf::tinygltf'")

# tinygltf has its own version of json and stb headers in the same folder,
# so to avoid potential conflicts we just download the one header we want.
set(TINYGLTF_VERSION "v2.8.3")
set(TINYGLTF_URL "https://raw.githubusercontent.com/syoyo/tinygltf/${TINYGLTF_VERSION}/tiny_gltf.h")

include(CPM)
CPMAddPackage(
    NAME tinygltf
    URL ${TINYGLTF_URL}
    DOWNLOAD_NO_EXTRACT 1
    DOWNLOAD_ONLY ON
)

# Implementation file
file(WRITE "${tinygltf_BINARY_DIR}/tinygltf.cpp.in" [[
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_write.h>
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
]])
configure_file(${tinygltf_BINARY_DIR}/tinygltf.cpp.in ${tinygltf_BINARY_DIR}/tinygltf.cpp COPYONLY)

# define library
add_library(tinygltf ${tinygltf_BINARY_DIR}/tinygltf.cpp)
add_library(tinygltf::tinygltf ALIAS tinygltf)
target_compile_definitions(tinygltf PRIVATE
    TINYGLTF_NO_INCLUDE_JSON
    TINYGLTF_NO_INCLUDE_STB_IMAGE
    TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE)
set_target_properties(tinygltf PROPERTIES FOLDER third_party)
set_target_properties(tinygltf PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
target_include_directories(tinygltf PUBLIC
    $<BUILD_INTERFACE:${tinygltf_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include(stb)
include(nlohmann_json)
target_link_libraries(tinygltf PRIVATE stb::image stb::image_write nlohmann_json::nlohmann_json)

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME tinygltf)
install(TARGETS tinygltf EXPORT Tinygltf_Targets)
install(EXPORT Tinygltf_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinygltf NAMESPACE tinygltf::)
install(DIRECTORY ${tinygltf_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
