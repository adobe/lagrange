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
if(TARGET nasoq::nasoq)
    return()
endif()

# note:
# you may want to link to nasoq::eigen_interface rather than nasoq::nasoq.
# nasoq::eigen_interface depends on nasoq::nasoq.
# nasoq::nasoq does NOT include nasoq::eigen_interface.

include(blas)
include(eigen)
include(metis)

option(NASOQ_WITH_EIGEN "Build NASOQ Eigen interface" ON)

# Note: For now, Nasoq's CMake code to find OpenBLAS is broken on Linux, so we default to MKL.
if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm64" OR "${CMAKE_OSX_ARCHITECTURES}" MATCHES "arm64")
    # apple M1
    set(NASOQ_BLAS_BACKEND "OpenBLAS" CACHE STRING "BLAS implementation for NASOQ to use")
else()
    # windows, linux, apple intel
    set(NASOQ_BLAS_BACKEND "MKL" CACHE STRING "BLAS implementation for NASOQ to use")
endif()

if(NASOQ_BLAS_BACKEND STREQUAL "MKL")
    include(mkl)
elseif(NASOQ_BLAS_BACKEND STREQUAL "OpenBLAS")
    option(NASOQ_USE_CLAPACK "Use CLAPACK as the LAPACK implementation" ON)
    include(openblas)
endif()

message(STATUS "Third-party (external): creating target 'nasoq::nasoq'")

set(CMAKE_DISABLE_FIND_PACKAGE_OpenMP TRUE)
include(CPM)
CPMAddPackage(
    NAME nasoq
    GITHUB_REPOSITORY sympiler/nasoq
    GIT_TAG 3565bba5c984e24a1e22f3555e2ba09e31c4486f
)

target_link_libraries(nasoq PUBLIC BLAS::BLAS)

set_target_properties(nasoq PROPERTIES FOLDER third_party)
if(TARGET clapack)
    set_target_properties(clapack PROPERTIES FOLDER third_party)
endif()
