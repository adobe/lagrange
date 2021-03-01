#
# Copyright 2020 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET lagrange::warnings)
    return()
endif()

# interface target for our warnings
add_library(lagrange_warnings INTERFACE)
add_library(lagrange::warnings ALIAS lagrange_warnings)

# installation
set_target_properties(lagrange_warnings PROPERTIES EXPORT_NAME warnings)
install(TARGETS lagrange_warnings EXPORT Lagrange_Targets)

# options
# More options can be found at:
# https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html#variable:CMAKE_%3CLANG%3E_COMPILER_ID
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    target_compile_options(lagrange_warnings INTERFACE
        "-Wall"
        "-Wno-missing-braces"
        "-Wmissing-field-initializers"
        "-Wno-unknown-pragmas"
        "-Wshadow"
        "-Werror=c++17-extensions"
        "-Werror=c++2a-extensions"
        "-msse3"
    )

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(lagrange_warnings INTERFACE
        "-Wall"
        "-Wno-missing-braces"
        "-Wmissing-field-initializers"
        "-Wno-unknown-pragmas"
        "-Wshadow"
        "-Werror=c++17-extensions"
        "-Werror=c++2a-extensions"
        "-msse3"
    )

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # using GCC
    target_compile_options(lagrange_warnings INTERFACE
        "-Wall"
        "-Wextra"
        "-Wno-unknown-pragmas"
        "-Wshadow"
        "-msse3"
    )

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    # using Intel C++

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # using Visual Studio C++ on Windows
    target_compile_definitions(lagrange_warnings INTERFACE _USE_MATH_DEFINES)
    target_compile_definitions(lagrange_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(lagrange_warnings INTERFACE _ENABLE_EXTENDED_ALIGNED_STORAGE)
    target_compile_definitions(lagrange_warnings INTERFACE NOMINMAX)

    # using /Wall on Visual Studio is a bad idea, since it enables all warnings that are disabled by default. /W4 is for lint-like warnings.
    # https://docs.microsoft.com/en-us/previous-versions/thxezb7y(v=vs.140)
    target_compile_options(lagrange_warnings INTERFACE /W3)

    # disabling "Unknown pragma" warnings
    target_compile_options(lagrange_warnings INTERFACE /wd4068)

    # Adding /bigobj. This is currently required to build Lagrange on windows with debug info.
    # https://docs.microsoft.com/en-us/cpp/build/reference/bigobj-increase-number-of-sections-in-dot-obj-file
    target_compile_options(lagrange_warnings INTERFACE /bigobj)
endif()

