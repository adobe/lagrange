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
if(TARGET LAPACK::LAPACK)
    return()
endif()

message(STATUS "Third-party (external): creating target 'LAPACK::LAPACK'")

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64" OR "${CMAKE_OSX_ARCHITECTURES}" MATCHES "arm64")
    # Use Accelerate on macOS M1
    set(BLA_VENDOR Apple)
    find_package(LAPACK REQUIRED)
else()
    # Use MKL on other platforms
    include(mkl)
    add_library(LAPACK::LAPACK ALIAS mkl::mkl)
endif()
