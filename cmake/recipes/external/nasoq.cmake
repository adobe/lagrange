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


include(eigen)
include(metis)

option(NASOQ_WITH_EIGEN "Build NASOQ Eigen interface" ON)

# Note: For now, Nasoq's CMake code to find OpenBLAS is broken on Linux, so we default to MKL.
set(NASOQ_BLAS_BACKEND "MKL" CACHE STRING "BLAS implementation for NASOQ to use")

if(NASOQ_BLAS_BACKEND STREQUAL "MKL")
    include(mkl)
elseif(NASOQ_BLAS_BACKEND STREQUAL "OpenBLAS")
    option(NASOQ_USE_CLAPACK "Use CLAPACK as the LAPACK implementation" ON)
    include(openblas)
endif()

message(STATUS "Third-party (external): creating target 'nasoq::nasoq'")

include(FetchContent)
FetchContent_Declare(
    nasoq
    GIT_REPOSITORY https://github.com/sympiler/nasoq.git
    GIT_TAG 3565bba5c984e24a1e22f3555e2ba09e31c4486f
)
set(CMAKE_DISABLE_FIND_PACKAGE_OpenMP TRUE)
FetchContent_MakeAvailable(nasoq)

if(NASOQ_BLAS_BACKEND STREQUAL "MKL")
    # Really should be handled by Nasoq's CMake...
    target_link_libraries(nasoq PUBLIC mkl::mkl)
endif()

set_target_properties(nasoq PROPERTIES FOLDER third_party)
if(TARGET clapack)
    set_target_properties(clapack PROPERTIES FOLDER third_party)
endif()
