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

lagrange_find_package(Eigen3 REQUIRED)
include(metis)

option(NASOQ_WITH_EIGEN "Build NASOQ Eigen interface" ON)

include(blas)
include(lapack)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR "arm64" IN_LIST CMAKE_OSX_ARCHITECTURES)
    # Change to accelerate after https://github.com/sympiler/nasoq/pull/27 is merged
    set(NASOQ_BLAS_BACKEND "OpenBLAS" CACHE STRING "BLAS implementation for NASOQ to use")
    set(NASOQ_USE_CLAPACK ON)
    include(openblas)
else()
    # use MKL on windows, linux, apple intel
    set(NASOQ_BLAS_BACKEND "MKL" CACHE STRING "BLAS implementation for NASOQ to use")
    add_library(mkl::mkl ALIAS MKL::MKL) # TODO: Patch Nasoq to use MKL::MKL target name
endif()

message(STATUS "Third-party (external): creating target 'nasoq::nasoq'")

set(CMAKE_DISABLE_FIND_PACKAGE_OpenMP TRUE)
include(CPM)
CPMAddPackage(
    NAME nasoq
    GITHUB_REPOSITORY sympiler/nasoq
    GIT_TAG 03bc29fc10fcc29b68e59fa18b081a1a45779e9b
)

target_link_libraries(nasoq PUBLIC BLAS::BLAS)

set_target_properties(nasoq PROPERTIES FOLDER third_party)
if(TARGET clapack)
    set_target_properties(clapack PROPERTIES FOLDER third_party)
endif()
