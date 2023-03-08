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
if(TARGET embree::embree)
    return()
endif()

message(STATUS "Third-party (external): creating target 'embree::embree'")

include(FetchContent)
FetchContent_Declare(
    embree
    GIT_REPOSITORY https://github.com/embree/embree.git
    GIT_TAG        v3.13.5
    GIT_SHALLOW    TRUE
)

# Set Embree's default options
option(EMBREE_ISPC_SUPPORT   "Build Embree with support for ISPC applications." OFF)
option(EMBREE_TUTORIALS      "Enable to build Embree tutorials"                 OFF)
option(EMBREE_STATIC_LIB     "Build Embree as a static library."                ON)
set(EMBREE_TESTING_INTENSITY 0         CACHE STRING "Intensity of testing (0 = no testing, 1 = verify and tutorials, 2 = light testing, 3 = intensive testing.")
set(EMBREE_TASKING_SYSTEM    "TBB"     CACHE STRING "Selects tasking system")

# The following options are necessary to ensure packed-ray support
option(EMBREE_RAY_MASK       "Enable the usage of mask for packed ray."         ON)
option(EMBREE_RAY_PACKETS    "Enable the usage packed ray."                     ON)

if(APPLE)
    set(EMBREE_MAX_ISA "NEON" CACHE STRING "Selects highest ISA to support.")
elseif(EMSCRIPTEN)
    set(EMBREE_MAX_ISA "SSE2" CACHE STRING "Selects highest ISA to support.")
else()
    set(EMBREE_MAX_ISA "DEFAULT" CACHE STRING "Selects highest ISA to support.")
endif()

# We want to compile Embree with TBB support, so we need to overwrite Embree's
# `find_package()` and provide variables. The following discussion provide some
# context on how to achieve this:
# - https://gitlab.kitware.com/cmake/cmake/issues/17735
# - https://crascit.com/2018/09/14/do-not-redefine-cmake-commands/
function(embree_import_target)
    macro(push_variable var value)
        if(DEFINED CACHE{${var}})
            set(LAGRANGE_OLD_${var}_VALUE "${${var}}")
            set(LAGRANGE_OLD_${var}_TYPE CACHE_TYPE)
        elseif(DEFINED ${var})
            set(LAGRANGE_OLD_${var}_VALUE "${${var}}")
            set(LAGRANGE_OLD_${var}_TYPE NORMAL_TYPE)
        else()
            set(LAGRANGE_OLD_${var}_TYPE NONE_TYPE)
        endif()
        set(${var} "${value}" CACHE PATH "" FORCE)
    endmacro()

    macro(pop_variable var)
        if(LAGRANGE_OLD_${var}_TYPE STREQUAL CACHE_TYPE)
            set(${var} "${LAGRANGE_OLD_${var}_VALUE}" CACHE PATH "" FORCE)
        elseif(LAGRANGE_OLD_${var}_TYPE STREQUAL NORMAL_TYPE)
            unset(${var} CACHE)
            set(${var} "${LAGRANGE_OLD_${var}_VALUE}")
        elseif(LAGRANGE_OLD_${var}_TYPE STREQUAL NONE_TYPE)
            unset(${var} CACHE)
        else()
            message(FATAL_ERROR "Trying to pop a variable that has not been pushed: ${var}")
        endif()
    endmacro()

    macro(ignore_package NAME)
        set(LAGRANGE_DUMMY_DIR "${CMAKE_CURRENT_BINARY_DIR}/embree_cmake/${NAME}")
        file(WRITE ${LAGRANGE_DUMMY_DIR}/${NAME}Config.cmake "")
        push_variable(${NAME}_DIR ${LAGRANGE_DUMMY_DIR})
        push_variable(${NAME}_ROOT ${LAGRANGE_DUMMY_DIR})
    endmacro()

    macro(unignore_package NAME)
        pop_variable(${NAME}_DIR)
        pop_variable(${NAME}_ROOT)
    endmacro()

    # Prefer Config mode before Module mode to prevent embree from loading its own FindTBB.cmake
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

    # Embree wants to be able to export() its target, and expects a target named `TBB` to exist.
    # Somehow we stil need to define `TBB_INCLUDE_DIRS`, and linking against `TBB` isn't sufficient
    # to compile embree targets properly.
    include(tbb)
    ignore_package(TBB)
    get_target_property(TBB_INCLUDE_DIRS TBB::tbb INTERFACE_INCLUDE_DIRECTORIES)
    add_library(TBB INTERFACE)
    target_link_libraries(TBB INTERFACE TBB::tbb)
    set(TBB_LIBRARIES TBB)

    # Ready to include embree's atrocious CMake
    FetchContent_MakeAvailable(embree)

    unignore_package(TBB)

    # Disable warnings
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # Embree's subgrid.h is known for causing array subscript out of bound
        # warning.  Embree dev claim the code is correct and it is a GCC bug
        # for misfiring warnings.  See https://github.com/embree/embree/issues/271
        #
        # The issue should be fixed for gcc 9.2.1 and later.
        target_compile_options(embree PRIVATE "-Wno-array-bounds")
    endif()

    # Warning setting
    set(unix_compilers "AppleClang;Clang;GNU")
    if(CMAKE_CXX_COMPILER_ID IN_LIST unix_compilers) # IN_LIST wants the second arg to be a var
        target_compile_options(embree PRIVATE "-Wno-unused-private-field")
    endif()

    # Now we need to do some juggling to propagate the include directory properties
    # along with the `embree` target
    add_library(embree::embree INTERFACE IMPORTED GLOBAL)
    target_include_directories(embree::embree SYSTEM INTERFACE ${embree_SOURCE_DIR}/include)
    target_link_libraries(embree::embree INTERFACE embree)
endfunction()

# Call via a proper function in order to scope variables such as CMAKE_FIND_PACKAGE_PREFER_CONFIG and TBB_DIR
embree_import_target()

# Cleanup for IDEs
foreach(name IN ITEMS embree algorithms lexers math simd sys tasking uninstall)
    if(TARGET ${name})
        set_target_properties(${name} PROPERTIES FOLDER "third_party//embree")
    endif()
endforeach()
