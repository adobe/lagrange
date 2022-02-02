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

set(BLA_VENDOR OpenBLAS)
find_package(BLAS QUIET)
# note: to make the above find work you can manually install openblas.
# You can also just call `conda install openblas`.
# You may want to do that to avoid compiling openblas.

if(BLAS_FOUND)

    message(STATUS "Third-party (external): Creating target 'OpenBLAS::OpenBLAS' from imported BLAS::BLAS (${BLAS_LIBRARIES})")

    add_library(OpenBLAS INTERFACE)
    add_library(OpenBLAS::OpenBLAS ALIAS OpenBLAS)
    target_link_libraries(OpenBLAS INTERFACE BLAS::BLAS)

    # Hack: the found target has no headers, so we manually do that.
    # this might get fixed in cmake in the future, reference https://gitlab.kitware.com/cmake/cmake/-/issues/20268
    find_path(OPENBLAS_INCLUDE_PATH
        NAMES "openblas/cblas.h"
        REQUIRED)
    target_include_directories(OpenBLAS INTERFACE ${OPENBLAS_INCLUDE_PATH})

else()

    # Not doing anything here, we let nasoq's openblas.cmake handle it

endif()

