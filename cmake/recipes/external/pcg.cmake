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
if(TARGET pcg_cpp::pcg_cpp)
    return()
endif()

message(STATUS "Third-party (external): creating target 'pcg_cpp::pcg_cpp'")

include(CPM)
CPMAddPackage(
    NAME pcg
    GITHUB_REPOSITORY Total-Random/pcg-cpp
    GIT_TAG d91bcf5ae5559361ac0d3319d4ab95a231b32984
    DOWNLOAD_ONLY ON
)

add_library(pcg_cpp::pcg_cpp INTERFACE IMPORTED GLOBAL)
target_include_directories(pcg_cpp::pcg_cpp INTERFACE "${pcg_SOURCE_DIR}/include")

# PCG's auto-detection of endianness does not work properly for Windows on ARM.
# Set the endianness explicitly if CMake knows it.
if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    target_compile_definitions(pcg_cpp::pcg_cpp INTERFACE PCG_LITTLE_ENDIAN=0)
elseif(CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    target_compile_definitions(pcg_cpp::pcg_cpp INTERFACE PCG_LITTLE_ENDIAN=1)
endif()
