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
if(TARGET OpenVDB::openvdb)
    return()
endif()

message(STATUS "Third-party (external): creating target 'OpenVDB::openvdb'")

if(WIN32)
    # On Windows, prefer shared lib for OpenVDB, otherwise we can run into
    # linking error LNK1248 in Debug mode (due to lib size exceeding 4GB).
    option(OPENVDB_CORE_SHARED "" ON)
    option(OPENVDB_CORE_STATIC "" OFF)
else()
    option(OPENVDB_CORE_SHARED "" OFF)
    option(OPENVDB_CORE_STATIC "" ON)
endif()
option(OPENVDB_BUILD_CORE "" ON)
option(OPENVDB_BUILD_BINARIES "" OFF)
option(OPENVDB_ENABLE_RPATH "" OFF)

# option(USE_EXPLICIT_INSTANTIATION "" ON)

include(CMakeDependentOption)
cmake_dependent_option(OPENVDB_INSTALL_CMAKE_MODULES "" OFF "OPENVDB_BUILD_CORE" OFF)

# TODO: Enable Blosc/Zlib
option(USE_BLOSC "" OFF)
option(USE_ZLIB "" OFF)
option(USE_LOG4CPLUS "" OFF) # maybe later
option(USE_EXR "" OFF)
option(USE_CCACHE "" OFF)
option(USE_STATIC_DEPENDENCIES "" OFF)
option(DISABLE_CMAKE_SEARCH_PATHS "" ON)
option(DISABLE_DEPENDENCY_VERSION_CHECKS "" ON)
option(USE_PKGCONFIG "" OFF)
option(OPENVDB_DISABLE_BOOST_IMPLICIT_LINKING "" OFF)
option(OPENVDB_ENABLE_UNINSTALL "Adds a CMake uninstall target." OFF)
option(OPENVDB_FUTURE_DEPRECATION "Generate messages for upcoming deprecation" OFF)

if(NOT EMSCRIPTEN)
    set(OPENVDB_SIMD AVX CACHE STRING "")
endif()

if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
    # Workaround https://github.com/AcademySoftwareFoundation/openvdb/issues/1131
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if(USE_BLOSC)
    include(blosc)
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
        set(${var} "${value}")
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
    lagrange_find_package(TBB CONFIG REQUIRED)
    lagrange_find_package(Boost REQUIRED COMPONENTS
        algorithm
        any
        interprocess
        iostreams
        numeric_conversion
        uuid
    )
    include(ilmbase)
    ignore_package(TBB)
    ignore_package(Boost)

    if(USE_ZLIB)
        ignore_package(ZLIB)
        include(miniz)
        if(NOT TARGET ZLIB::ZLIB)
            get_target_property(_aliased miniz::miniz ALIASED_TARGET)
            add_library(ZLIB::ZLIB ALIAS ${_aliased})
        endif()
    endif()

    if(USE_BLOSC)
        ignore_package(Blosc)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "(Clang|GNU)")
        include(CheckCXXCompilerFlag)
        set(flag -Wno-missing-template-arg-list-after-template-kw)
        if(NOT DEFINED IS_SUPPORTED_${flag})
            check_cxx_compiler_flag("${flag}" IS_SUPPORTED_${flag})
        endif()
        if(IS_SUPPORTED_${flag})
            add_compile_options(${flag})
        endif()
    endif()

    # Ready to include openvdb CMake
    include(CPM)
    CPMAddPackage(
        NAME openvdb
        GITHUB_REPOSITORY AcademySoftwareFoundation/openvdb
        GIT_TAG v11.0.0
    )

    unignore_package(TBB)
    unignore_package(Boost)

    # Inject real Boost dependencies instead of dummy Boost:headers one
    foreach(name IN ITEMS openvdb_static openvdb_shared)
        if(TARGET ${name})
            target_link_libraries(${name}
                PUBLIC
                    Boost::algorithm
                    Boost::any
                    Boost::numeric_conversion
                    Boost::uuid
                PRIVATE
                    Boost::interprocess
            )
            target_compile_definitions(${name} PRIVATE BOOST_ALL_NO_LIB)
        endif()
    endforeach()

    # Forward ALIAS target for openvdb
    get_target_property(_aliased openvdb ALIASED_TARGET)
    if(_aliased)
        message(STATUS "Creating 'OpenVDB::openvdb' as a new ALIAS target for '${_aliased}'.")
        add_library(OpenVDB::openvdb ALIAS ${_aliased})
    else()
        add_library(OpenVDB::openvdb ALIAS openvdb)
    endif()

    # Copy miniz.h as zlib.h to have blosc use miniz symbols (which are aliased through #define in miniz.h)
    if(USE_ZLIB AND TARGET miniz::miniz)
        FetchContent_GetProperties(miniz)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include")
        configure_file(${miniz_SOURCE_DIR}/miniz.h ${CMAKE_CURRENT_BINARY_DIR}/include/zlib.h COPYONLY)

        get_target_property(_aliased OpenVDB::openvdb ALIASED_TARGET)
        include(GNUInstallDirs)
        target_include_directories(${_aliased} SYSTEM BEFORE PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
    endif()
endfunction()

# Call via a proper function in order to scope variables such as CMAKE_FIND_PACKAGE_PREFER_CONFIG and TBB_DIR
openvdb_import_target()

# Set folders for MSVC
foreach(name IN ITEMS openvdb_static openvdb_shared)
    if(TARGET ${name})
        set_target_properties(${name} PROPERTIES FOLDER third_party)
        set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endforeach()
