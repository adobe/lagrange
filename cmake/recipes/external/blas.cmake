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
if(TARGET BLAS::BLAS)
    return()
endif()

message(STATUS "Third-party (external): creating target 'BLAS::BLAS'")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR "arm64" IN_LIST CMAKE_OSX_ARCHITECTURES)
    # Use Accelerate on macOS M1
    set(BLA_VENDOR Apple)
    find_package(BLAS REQUIRED)
else()
    # Use MKL on other platforms
    lagrange_find_package(MKL CONFIG REQUIRED GLOBAL)
    add_library(BLAS::BLAS ALIAS MKL::MKL)
endif()
