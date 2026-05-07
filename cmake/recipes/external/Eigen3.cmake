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
if(TARGET Eigen3::Eigen)
    return()
endif()

option(EIGEN_WITH_MKL "Use Eigen with MKL" OFF)
option(EIGEN_DONT_VECTORIZE "Disable Eigen vectorization" OFF)

message(STATUS "Third-party (external): creating target 'Eigen3::Eigen'")

set(EIGEN_VERSION "5.0.1" CACHE STRING "Version of Eigen to use")

include(CPM)
CPMAddPackage(
    NAME eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG ${EIGEN_VERSION}
    DOWNLOAD_ONLY ON
)
FetchContent_GetProperties(eigen)
set(EIGEN_INCLUDE_DIRS ${eigen_SOURCE_DIR})

add_library(Eigen3_Eigen INTERFACE)
add_library(Eigen3::Eigen ALIAS Eigen3_Eigen)

include(GNUInstallDirs)
target_include_directories(Eigen3_Eigen SYSTEM INTERFACE
    $<BUILD_INTERFACE:${EIGEN_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Not necessary after Eigen 5, but required for older versions. Doesn't hurt to keep it.
target_compile_definitions(Eigen3_Eigen INTERFACE EIGEN_MPL2_ONLY)

if(EIGEN_DONT_VECTORIZE)
    target_compile_definitions(Eigen3_Eigen INTERFACE EIGEN_DONT_VECTORIZE)
endif()

# Diagnostic only — TEMPORARY: on Windows ARM64 Debug, force-include a header that overrides
# eigen_assert with a non-fatal handler. When the assertion comes from DenseStorage.h (i.e. the
# plain_array<> alignment check) we capture a cpptrace stack trace so we can pinpoint the call
# site that constructs a misaligned fixed-size Eigen object. All other eigen_assert failures
# still abort, so unrelated invariants are not masked.
#
# The cpptrace dependency stays in a separate static library (lagrange_eigen_align_diag) so it
# does not leak into every Eigen consumer's interface.
if(WIN32 AND CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" AND MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    include(cpptrace)
    add_library(lagrange_eigen_align_diag STATIC
        ${CMAKE_CURRENT_LIST_DIR}/eigen_alignment_diag.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eigen_alignment_diag.h
    )
    target_include_directories(lagrange_eigen_align_diag PUBLIC ${CMAKE_CURRENT_LIST_DIR})
    target_compile_definitions(lagrange_eigen_align_diag PUBLIC LAGRANGE_DIAG_EIGEN_ALIGN=1)
    target_link_libraries(lagrange_eigen_align_diag PRIVATE cpptrace::cpptrace)
    set_target_properties(lagrange_eigen_align_diag PROPERTIES FOLDER third_party)

    set(_lagrange_eigen_diag_header "${CMAKE_CURRENT_LIST_DIR}/eigen_alignment_diag.h")
    target_compile_options(Eigen3_Eigen INTERFACE "/FI${_lagrange_eigen_diag_header}")
    target_compile_definitions(Eigen3_Eigen INTERFACE LAGRANGE_DIAG_EIGEN_ALIGN=1)
    target_link_libraries(Eigen3_Eigen INTERFACE lagrange_eigen_align_diag)
endif()

if(EIGEN_WITH_MKL)
    # TODO: Checks that, on 64bits systems, `MKL::MKL` is using the LP64 interface
    # (by looking at the compile definition of the target)
    lagrange_find_package(MKL CONFIG REQUIRED GLOBAL)
    target_link_libraries(Eigen3_Eigen INTERFACE MKL::MKL)
    target_compile_definitions(Eigen3_Eigen INTERFACE
        EIGEN_USE_MKL_ALL
        EIGEN_USE_LAPACKE_STRICT
    )
endif()

# On Windows, enable natvis files to improve debugging experience
if(WIN32 AND eigen_SOURCE_DIR)
    target_sources(Eigen3_Eigen INTERFACE $<BUILD_INTERFACE:${eigen_SOURCE_DIR}/debug/msvc/eigen.natvis>)
endif()

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME eigen)
set_target_properties(Eigen3_Eigen PROPERTIES EXPORT_NAME Eigen)
install(DIRECTORY ${EIGEN_INCLUDE_DIRS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS Eigen3_Eigen EXPORT Eigen_Targets)
install(EXPORT Eigen_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eigen NAMESPACE Eigen3::)
