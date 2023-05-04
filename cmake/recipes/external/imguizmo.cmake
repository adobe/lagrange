#
# Copyright 2019 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET imguizmo::imguizmo)
    return()
endif()

message(STATUS "Third-party (external): creating target 'imguizmo::imguizmo'")

include(CPM)
CPMAddPackage(
    NAME imguizmo
    GITHUB_REPOSITORY CedricGuillemet/ImGuizmo
    GIT_TAG e3174578bdc99c715e51c5ad88e7d50b4eeb19b0
)

add_library(imguizmo
    "${imguizmo_SOURCE_DIR}/ImGuizmo.h"
    "${imguizmo_SOURCE_DIR}/ImGuizmo.cpp"
)
add_library(imguizmo::imguizmo ALIAS imguizmo)

target_include_directories(imguizmo PUBLIC "${imguizmo_SOURCE_DIR}")

include(imgui)
target_link_libraries(imguizmo PUBLIC imgui::imgui)

set_target_properties(imguizmo PROPERTIES FOLDER third_party)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    target_compile_options(imguizmo PRIVATE
        "-Wno-unused-const-variable" "-Wno-unused-function"
    )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(imguizmo PRIVATE
        "-Wno-unused-const-variable" "-Wno-unused-function"
    )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(imguizmo PRIVATE
        "-Wno-unused-const-variable" "-Wno-unused-function" "-Wno-deprecated-copy"
    )
endif()
