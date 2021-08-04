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
if(TARGET openvdb::openvdb)
    return()
endif()

message(STATUS "Third-party (external): creating target 'openvdb::openvdb'")

include(FetchContent)
FetchContent_Declare(
    openvdb
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openvdb.git
    GIT_TAG 05b32911c2699b2cfe4b364da071cca9fcc420f2
)

option(OPENVDB_CORE_SHARED "" OFF)
option(OPENVDB_CORE_STATIC "" ON)
option(OPENVDB_BUILD_CORE "" ON)
option(OPENVDB_BUILD_BINARIES "" OF)
option(OPENVDB_ENABLE_RPATH "" OFF)
cmake_dependent_option(OPENVDB_INSTALL_CMAKE_MODULES "" OFF "OPENVDB_BUILD_CORE" OFF)

option(USE_BLOSC "" OFF) # maybe later
option(USE_ZLIB "" OFF) # maybe later
option(USE_LOG4CPLUS "" OFF) # maybe later
option(USE_EXR "" OFF)
option(USE_CCACHE "" OFF)
option(USE_STATIC_DEPENDENCIES "" OFF)
option(DISABLE_CMAKE_SEARCH_PATHS "" ON)
option(DISABLE_DEPENDENCY_VERSION_CHECKS "" ON)
option(OPENVDB_FUTURE_DEPRECATION "" ON)
option(USE_PKGCONFIG "" OFF)
option(OPENVDB_DISABLE_BOOST_IMPLICIT_LINKING "" OFF)
option(OPENVDB_ENABLE_UNINSTALL "Adds a CMake uninstall target." OFF)

set(OPENVDB_SIMD AVX CACHE STRING "")

if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
    # Workaround https://github.com/AcademySoftwareFoundation/openvdb/issues/1131
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Note: OpenVDB will link against TBB::tbbmalloc or TBB::tbbmalloc_proxy if available.
# We may want to use that in our host application to improve performances.
# set(CONCURRENT_MALLOC Auto CACHE STRING "")

# We want to compile OpenVDB with TBB support, so we need to overwrite OpenVDB's
# `find_package()` and provide variables. The following discussion provide some
# context on how to achieve this:
# - https://gitlab.kitware.com/cmake/cmake/issues/17735
# - https://crascit.com/2018/09/14/do-not-redefine-cmake-commands/
function(openvdb_import_target)
    macro(push_variable var value)
        if(DEFINED CACHE{${var}})
            set(OPENVDB_OLD_${var}_VALUE "${${var}}")
            set(OPENVDB_OLD_${var}_TYPE CACHE_TYPE)
        elseif(DEFINED ${var})
            set(OPENVDB_OLD_${var}_VALUE "${${var}}")
            set(OPENVDB_OLD_${var}_TYPE NORMAL_TYPE)
        else()
            set(OPENVDB_OLD_${var}_TYPE NONE_TYPE)
        endif()
        set(${var} "${value}" CACHE PATH "" FORCE)
    endmacro()

    macro(pop_variable var)
        if(OPENVDB_OLD_${var}_TYPE STREQUAL CACHE_TYPE)
            set(${var} "${OPENVDB_OLD_${var}_VALUE}" CACHE PATH "" FORCE)
        elseif(OPENVDB_OLD_${var}_TYPE STREQUAL NORMAL_TYPE)
            unset(${var} CACHE)
            set(${var} "${OPENVDB_OLD_${var}_VALUE}")
        elseif(OPENVDB_OLD_${var}_TYPE STREQUAL NONE_TYPE)
            unset(${var} CACHE)
        else()
            message(FATAL_ERROR "Trying to pop a variable that has not been pushed: ${var}")
        endif()
    endmacro()

    macro(ignore_package NAME)
        set(OPENVDB_DUMMY_DIR "${CMAKE_CURRENT_BINARY_DIR}/openvdb_cmake/${NAME}")
        file(WRITE ${OPENVDB_DUMMY_DIR}/${NAME}Config.cmake "")
        push_variable(${NAME}_DIR ${OPENVDB_DUMMY_DIR})
        push_variable(${NAME}_ROOT ${OPENVDB_DUMMY_DIR})
    endmacro()

    macro(unignore_package NAME)
        pop_variable(${NAME}_DIR)
        pop_variable(${NAME}_ROOT)
    endmacro()

    # Prefer Config mode before Module mode to prevent embree from loading its own FindTBB.cmake
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

    # Import our own targets
    ignore_package(TBB)
    ignore_package(Boost)
    ignore_package(IlmBase)
    include(tbb)
    include(boost)
    include(ilmbase)
    set(Tbb_VERSION 2019.0 CACHE STRING "" FORCE)
    set(Boost_LIB_VERSION 1.70 CACHE STRING "" FORCE)
    set(IlmBase_VERSION 2.4 CACHE STRING "" FORCE)

    # Ready to include openvdb CMake
    FetchContent_MakeAvailable(openvdb)

    unignore_package(TBB)
    unignore_package(Boost)
    unignore_package(IlmBase)

    # Forward ALIAS target for openvdb
    get_target_property(_aliased openvdb ALIASED_TARGET)
    if(_aliased)
        message(STATUS "Creating 'openvdb::openvdb' as a new ALIAS target for '${_aliased}'.")
        add_library(openvdb::openvdb ALIAS ${_aliased})
    else()
        add_library(openvdb::openvdb ALIAS openvdb)
    endif()
endfunction()

# Call via a proper function in order to scope variables such as CMAKE_FIND_PACKAGE_PREFER_CONFIG and TBB_DIR
openvdb_import_target()
