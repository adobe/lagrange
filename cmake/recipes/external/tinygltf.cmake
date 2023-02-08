#
# Copyright 2022 Adobe. All rights reserved.
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

include(FetchContent)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.7.0
    GIT_SHALLOW TRUE
)

option(TINYGLTF_BUILD_LOADER_EXAMPLE "Build loader_example(load glTF and dump infos)" OFF)
option(TINYGLTF_BUILD_GL_EXAMPLES "Build GL exampels(requires glfw, OpenGL, etc)" OFF)
option(TINYGLTF_BUILD_VALIDATOR_EXAMPLE "Build validator exampe" OFF)
option(TINYGLTF_BUILD_BUILDER_EXAMPLE "Build glTF builder example" OFF)
option(TINYGLTF_HEADER_ONLY "On: header-only mode. Off: create tinygltf library(No TINYGLTF_IMPLEMENTATION required in your project)" OFF)
option(TINYGLTF_INSTALL "Install tinygltf files during install step. Usually set to OFF if you include tinygltf through add_subdirectory()" ON)
FetchContent_MakeAvailable(tinygltf)

set_target_properties(tinygltf PROPERTIES FOLDER third_party)

add_library(tinygltf::tinygltf ALIAS tinygltf)
