#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    return()
endif()

# First let's check if we are using the classical linker with Xcode 15+
set(is_using_classical_linker TRUE)
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0.0)
    set(is_using_classical_linker FALSE)
    foreach(type IN ITEMS EXE SHARED MODULE)
        set(suffix_types "none")
        if(DEFINED CMAKE_CONFIGURATION_TYPES)
            list(APPEND suffix_types ${CMAKE_CONFIGURATION_TYPES})
        endif()
        foreach(config IN LISTS suffix_types)
            if(config STREQUAL "none")
                set(config "")
            else()
                set(config "_${config}")
            endif()
            if(NOT DEFINED CMAKE_${type}_LINKER_FLAGS${config}_INIT)
                continue()
            endif()
            if(CMAKE_${type}_LINKER_FLAGS${config}_INIT MATCHES "-Wl,-ld_classic")
                set(is_using_classical_linker TRUE)
                break()
            endif()
        endforeach()
    endforeach()
endif()

# Xcode 15's new linker will crash. We suggest either using the classical linker, or upgrading to Xcode 15.1.
# AppleClang Compiler ID for Xcode 15.0 is 15.0.0.15000040
# AppleClang Compiler ID for Xcode 15.1 and 15.2 is 15.0.0.15000100
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0.0.15000040 AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0.0.15000100
)
    # If using the classical linker, display an error message
    if(NOT is_using_classical_linker)
        option(LAGRANGE_IGNORE_XCODE_15_LINKER_ERROR "Turn Xcode 15 linker check from an error into a warning" OFF)
        if(LAGRANGE_IGNORE_XCODE_15_LINKER_ERROR)
            set(mode WARNING)
            set(postfix "")
        else()
            set(mode FATAL_ERROR)
            set(postfix "You can also ignore this error by calling CMake with -DLAGRANGE_IGNORE_XCODE_15_LINKER_ERROR=ON\n")
        endif()
        message(${mode}
            "Xcode 15.0 has a new linker that will cause a crash when linking weak symbols in C++. "
            "We suggest either:\n1. Upgrading to Xcode >= 15.1, or\n2. Using the classical linker by deleting "
            "your <build>/CMakeCache.txt and re-running CMake configure with \n"
            "    LDFLAGS=\"$LDFLAGS -Wl,-ld_classic\" cmake ..\n"
            ${postfix}
        )
    endif()
endif()

# Fix linker warnings with Xcode 13+. Example warning:
#
#     ld: warning: direct access in function 'xxx' from file 'yyy.cpp.o' to global weak symbol 'typeinfo for
#     lagrange::Attribute<signed char>' from file 'modules/core/Release/liblagrange_core.a(Attribute.cpp.o)' means the
#     weak symbol cannot be overridden at runtime. This was likely caused by different translation units being compiled
#     with different visibility settings.
#
if(LAGRANGE_TOPLEVEL_PROJECT AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0.0 AND
    is_using_classical_linker AND
    NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET
)
    # Currently, OneTBB cannot be built with hidden visibility (TBB 2020 has a similar issue):
    # https://github.com/oneapi-src/oneTBB/issues/713
    # https://github.com/oneapi-src/oneTBB/pull/1114
    # message(WARNING
    #     "Using Xcode 13+ or Xcode 15+ with the classical linker. By default Lagrange will compile with "
    #     "-fvisibility=hidden to avoid linker warnings. If you encounter issues, please set "
    #     "CMAKE_CXX_VISIBILITY_PRESET explicitly when configuring Lagrange"
    # )
    # set(CMAKE_CXX_VISIBILITY_PRESET hidden)
endif()
