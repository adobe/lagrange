#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
function(lagrange_find_package name)

    # Package which may be found via find_package() when LAGRANGE_USE_FIND_PACKAGE is ON
    set(maybe_external_packages
        algcmake
        Boost
        Eigen3
        embree
        MKL
        OpenSubdiv
        OpenVDB
        # pxr
        span-lite
        spdlog
        TBB
    )

    # Defer to find_package() if desired. In the future we will switch to a dependency provider.
    if(LAGRANGE_USE_FIND_PACKAGE AND ${name} IN_LIST maybe_external_packages)
        # Once we update to MKL 2024.2.1 on vcpkg, we should be able to use their MKLConfig.cmake
        # directly. Until then, we need to guard against multiple inclusion by checking MKL::MKL.
        # https://github.com/oneapi-src/oneMKL/pull/568
        if(${name} STREQUAL MKL)
            if(NOT TARGET MKL::MKL)
                find_package(${name} ${ARGN})
            endif()
        elseif(${name} STREQUAL TBB)
            find_package(${name} ${ARGN})
            # TODO: Move that to our vcpkg port?
            target_compile_definitions(TBB::tbb INTERFACE __TBB_NO_IMPLICIT_LINKAGE=1)
        elseif(${name} STREQUAL OpenVDB)
            find_package(${name} ${ARGN})
            # https://github.com/AcademySoftwareFoundation/openvdb/issues/1630
            if(NOT BUILD_SHARED_LIBS)
                find_package(blosc CONFIG REQUIRED)
                find_package(ZLIB REQUIRED)
                target_link_libraries(OpenVDB::openvdb INTERFACE blosc_static ZLIB::ZLIB)
            endif()
        elseif(${name} STREQUAL algcmake)
            find_package(${name} ${ARGN})
            set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
        else()
            find_package(${name} ${ARGN})
        endif()
        if(${name} STREQUAL pxr AND NOT TARGET usd::usd)
            add_library(usd::usd ALIAS usd)
            add_library(usd::usdGeom ALIAS usdGeom)
        endif()
        if(${name} STREQUAL embree AND NOT TARGET embree::embree)
            add_library(embree::embree ALIAS embree) # TODO: Fix port instead?
        endif()
    else()
        if(${name} STREQUAL algcmake)
            include(3di/algcmake)
            FetchContent_GetProperties(algcmake)

            list(APPEND CMAKE_MODULE_PATH
                ${algcmake_SOURCE_DIR}
                ${algcmake_SOURCE_DIR}/finds
                ${algcmake_SOURCE_DIR}/modules
            )
            set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
        else()
            include(${name})
        endif()
    endif()

endfunction()
