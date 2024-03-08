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

# Xcode 15's new linker will crash. We suggest either using the classical linker, or upgrading to Xcode 15.1.
# AppleClang Compiler ID for Xcode 15.0 is 15.0.0.15000040
# AppleClang Compiler ID for Xcode 15.1 and 15.2 is 15.0.0.15000100
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0.0.15000040 AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0.0.15000100
)
    # First let's check if we are using the classical linker
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

    # If not, display an error message
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
