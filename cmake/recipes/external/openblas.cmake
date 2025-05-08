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
if(TARGET OpenBLAS::OpenBLAS)
    return()
endif()

message(STATUS "Third-party (external): creating target 'OpenBLAS::OpenBLAS'")

block()
    set(BUILD_WITHOUT_LAPACK ON)
    set(BUILD_WITHOUT_LAPACKE ON)
    set(BUILD_TESTING OFF)
    set(BUILD_BENCHMARKS OFF)
    set(USE_OPENMP OFF)
    set(NOFORTRAN ON)

    include(CPM)
    CPMAddPackage(
        NAME OpenBLAS
        GITHUB_REPOSITORY OpenMathLib/OpenBLAS
        GIT_TAG v0.3.29
    )

    set(OpenBLAS_LIBNAME openblas${SUFFIX64_UNDERSCORE})
    if(NOT TARGET OpenBLAS::OpenBLAS)
        get_target_property(_aliased ${OpenBLAS_LIBNAME} ALIASED_TARGET)
        if(_aliased)
            message(STATUS "Creating 'OpenBLAS::OpenBLAS' as a new ALIAS target for '${_aliased}'.")
            add_library(OpenBLAS::OpenBLAS ALIAS ${_aliased})
            set(OpenBLAS_LIBNAME ${_aliased})
        else()
            add_library(OpenBLAS::OpenBLAS ALIAS ${OpenBLAS_LIBNAME})
        endif()
    endif()
    set_target_properties(${OpenBLAS_LIBNAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # Copy generated headers into expected subfolder
    file(MAKE_DIRECTORY "${OpenBLAS_BINARY_DIR}/include/openblas")
    configure_file(${CMAKE_BINARY_DIR}/generated/cblas.h "${OpenBLAS_BINARY_DIR}/include/openblas/cblas.h" COPYONLY)
    configure_file(${CMAKE_BINARY_DIR}/openblas_config.h "${OpenBLAS_BINARY_DIR}/include/openblas_config.h" COPYONLY)

    include(GNUInstallDirs)
    target_include_directories(${OpenBLAS_LIBNAME} INTERFACE
        $<BUILD_INTERFACE:${OpenBLAS_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

endblock()
