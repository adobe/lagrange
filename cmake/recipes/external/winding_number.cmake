#
# Copyright 2021 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET WindingNumber::WindingNumber)
    return()
endif()

message(STATUS "Third-party (external): creating target 'WindingNumber::WindingNumber'")

lagrange_find_package(TBB CONFIG REQUIRED)
include(simde)

include(CPM)
set(WINDINGNUMBER_PATCHES "")
if(MSVC AND CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
    # On MSVC ARM64, simde__m128 and simde__m128i are both __n128, making plain typedefs
    # identical and breaking all overloaded functions. Patch VM_SSEFunc.h to use distinct
    # wrapper structs instead. Use git apply --ignore-whitespace for robust CRLF handling.
    find_package(Git REQUIRED QUIET)
    set(_wn_patch "${CMAKE_CURRENT_LIST_DIR}/winding-number-winarm.patch")
    set(WINDINGNUMBER_PATCHES PATCH_COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${_wn_patch}")
endif()
CPMAddPackage(
    NAME WindingNumber
    GITHUB_REPOSITORY jdumas/WindingNumber
    GIT_TAG 81443613dd2ab66000249401f10589f9794dc048
)

set_target_properties(WindingNumber PROPERTIES FOLDER third_party)
