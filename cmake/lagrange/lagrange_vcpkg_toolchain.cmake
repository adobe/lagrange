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

# This file is meant to be used with the `special-vcpkg` preset.
block()
    list(PREPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/../recipes/external/"
    )
    include(lagrange_cpm_cache)
    include(CPM)
endblock()

if(NOT DEFINED ENV{VCPKG_DEFAULT_BINARY_CACHE})
    file(REAL_PATH "~/.cache/vcpkg/bin" VCPKG_DEFAULT_BINARY_CACHE EXPAND_TILDE)
    message(STATUS "Setting VCPKG_DEFAULT_BINARY_CACHE to ${VCPKG_DEFAULT_BINARY_CACHE}")
    file(MAKE_DIRECTORY "${VCPKG_DEFAULT_BINARY_CACHE}")
    set(ENV{VCPKG_DEFAULT_BINARY_CACHE} "${VCPKG_DEFAULT_BINARY_CACHE}")
endif()

if(NOT DEFINED ENV{VCPKG_DOWNLOADS})
    file(REAL_PATH "~/.cache/vcpkg/downloads" VCPKG_DOWNLOADS EXPAND_TILDE)
    message(STATUS "Setting VCPKG_DOWNLOADS to ${VCPKG_DOWNLOADS}")
    file(MAKE_DIRECTORY "${VCPKG_DOWNLOADS}")
    set(ENV{VCPKG_DOWNLOADS} "${VCPKG_DOWNLOADS}")
endif()

CPMAddPackage(
    NAME vcpkg
    GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
    GIT_TAG        2025.01.13
    QUIET
)

set(ENV{VCPKG_ROOT} "${vcpkg_SOURCE_DIR}")

# Since this file is included as a toolchain file via our CMake preset, we setup vcpkg.cmake
# via CMAKE_PROJECT_TOP_LEVEL_INCLUDES instead. See the following threads for more information:
# https://discourse.cmake.org/t/built-in-package-manager-for-cmake-modules/7513
# https://github.com/microsoft/vcpkg/discussions/26681
list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake")

if(${CMAKE_VERSION} VERSION_LESS "3.30.0")
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
else()
    set_property(GLOBAL PROPERTY PROPAGATE_TOP_LEVEL_INCLUDES_TO_TRY_COMPILE YES)
endif()
