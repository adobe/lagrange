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
if(TARGET mkl::mkl)
    return()
endif()

message(STATUS "Third-party (external): creating target 'mkl::mkl'")

# Interface layer: lp64 or ilp64
set(MKL_INTERFACE "lp64" CACHE STRING "Interface layer to use with MKL (lp64 or ilp64)")
set(MKL_INTERFACE_CHOICES ilp64 lp64)
set_property(CACHE MKL_INTERFACE PROPERTY STRINGS ${MKL_INTERFACE_CHOICES})
message(STATUS "MKL interface layer: ${MKL_INTERFACE}")

# Threading layer: sequential, tbb or openmp
set(MKL_THREADING "tbb" CACHE STRING "Threading layer to use with MKL (sequential, tbb or openmp)")
set(MKL_THREADING_CHOICES sequential tbb openmp)
set_property(CACHE MKL_THREADING PROPERTY STRINGS ${MKL_THREADING_CHOICES})
message(STATUS "MKL threading layer: ${MKL_THREADING}")

# Linking options
set(MKL_LINKING "static" CACHE STRING "Linking strategy to use with MKL (static, dynamic or sdl)")
set(MKL_LINKING_CHOICES static dynamic sdl)
set_property(CACHE MKL_LINKING PROPERTY STRINGS ${MKL_LINKINK_CHOICES})
message(STATUS "MKL linking strategy: ${MKL_LINKING}")

# MKL version
set(MKL_VERSION "2021.3.0" CACHE STRING "MKL version to use (2020.4 or 2021.3.0)")
set(MKL_VERSION_CHOICES 2020.4 2021.3.0)
set_property(CACHE MKL_VERSION PROPERTY STRINGS ${MKL_VERSION_CHOICES})
message(STATUS "MKL version: ${MKL_VERSION}")

################################################################################

# Check option for the interface layer
if(NOT MKL_INTERFACE IN_LIST MKL_INTERFACE_CHOICES)
    message(FATAL_ERROR "Unrecognized option for MKL_INTERFACE: ${MKL_INTERFACE}")
endif()

# Check option for the threading layer
if(MKL_THREADING STREQUAL "openmp")
    message(FATAL_ERROR "openmp dependency not implemented yet")
elseif(NOT MKL_THREADING IN_LIST MKL_THREADING_CHOICES)
    message(FATAL_ERROR "Unrecognized option for MKL_THREADING: ${MKL_THREADING}")
endif()

# Check option for the linking strategy
if(NOT MKL_LINKING IN_LIST MKL_LINKING_CHOICES)
    message(FATAL_ERROR "Unrecognized option for MKL_LINKING: ${MKL_LINKING}")
endif()

# Using the SDL lib requires linking against openmp, which we do not support
if(MKL_LINKING STREQUAL sdl)
    message(FATAL_ERROR "MKL_LINKING=sdl is not fully supported yet.")
endif()

# Check option for the library version
if(NOT MKL_VERSION IN_LIST MKL_VERSION_CHOICES)
    message(FATAL_ERROR "Unrecognized option for MKL_VERSION: ${MKL_VERSION}")
endif()

################################################################################

if(WIN32)
    set(MKL_PLATFORM win-64)
elseif(APPLE)
    set(MKL_PLATFORM osx-64)
elseif(UNIX)
    set(MKL_PLATFORM linux-64)
endif()

if(MKL_VERSION VERSION_EQUAL 2020.4)
    # To compute the md5 checksums for each lib, use the following bash script (replace the target version number):
    # for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-md5") <(conda search --override-channel --channel intel $f=2020.4 --platform $os-64 -i | grep md5 | cut -d : -f 2); done; done
    set(mkl-linux-64-md5 f85891f97a04c7b2fbf619d1b903d7f5)
    set(mkl-osx-64-md5 735d7f93c7fbcffe658f1ccf67418cb3)
    set(mkl-win-64-md5 37ade09cace5cd73053b16574a3ee3c3)
    set(mkl-include-linux-64-md5 8b2bf0e42bd95dd700d9877add1ca6de)
    set(mkl-include-osx-64-md5 26043328553952cdb064c5aab8b50c78)
    set(mkl-include-win-64-md5 87e9a73a6e6757a8ed0dbc87d50d7f60)
    set(mkl-static-linux-64-md5 9f589a1508fb083c3e73427db459ca4c)
    set(mkl-static-osx-64-md5 2f9e1b8b6d6b0903e81a573084e4494f)
    set(mkl-static-win-64-md5 5ae780c06edd0be62966c6d8ab47d5fb)
    set(mkl-devel-linux-64-md5 b571698ef237c0e61abe15b7d300f157)
    set(mkl-devel-osx-64-md5 ee58da0463676d910eeab9aec0470f0e)
    set(mkl-devel-win-64-md5 8a7736b81b9bc2d5c044b88d6ac8af6e)

    # To compute file names, we use the following bash script:
    # for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-file") <(conda search --override-channel --channel intel $f=2020.4 --platform $os-64 -i | grep file | cut -d : -f 2); done; done
    set(mkl-linux-64-file mkl-2020.4-intel_304.tar.bz2)
    set(mkl-osx-64-file mkl-2020.4-intel_301.tar.bz2)
    set(mkl-win-64-file mkl-2020.4-intel_311.tar.bz2)
    set(mkl-include-linux-64-file mkl-include-2020.4-intel_304.tar.bz2)
    set(mkl-include-osx-64-file mkl-include-2020.4-intel_301.tar.bz2)
    set(mkl-include-win-64-file mkl-include-2020.4-intel_311.tar.bz2)
    set(mkl-static-linux-64-file mkl-static-2020.4-intel_304.tar.bz2)
    set(mkl-static-osx-64-file mkl-static-2020.4-intel_301.tar.bz2)
    set(mkl-static-win-64-file mkl-static-2020.4-intel_311.tar.bz2)
    set(mkl-devel-linux-64-file mkl-devel-2020.4-intel_304.tar.bz2)
    set(mkl-devel-osx-64-file mkl-devel-2020.4-intel_301.tar.bz2)
    set(mkl-devel-win-64-file mkl-devel-2020.4-intel_311.tar.bz2)
elseif(MKL_VERSION VERSION_EQUAL 2021.3.0)
    # To compute the md5 checksums for each lib, use the following bash script (replace the target version number):
    # for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-md5") <(conda search --override-channel --channel intel $f=2021.3.0 --platform $os-64 -i | grep md5 | cut -d : -f 2); done; done
    set(mkl-linux-64-md5 2501643729c00b24fddb9530b339aea7)
    set(mkl-osx-64-md5 d6129ae9dfba58671667a65c160d0776)
    set(mkl-win-64-md5 264213ea4c5cb6b6d81ea97f59e757ab)
    set(mkl-include-linux-64-md5 70b4f9a53401a3d11ce27d7ddb0e2511)
    set(mkl-include-osx-64-md5 6da50c06992b78c4127a1881d39c1804)
    set(mkl-include-win-64-md5 28d785eb22d28512d4e40e5890a817dc)
    set(mkl-static-linux-64-md5 1469ad60a34269d4d0c5666bc131b82a)
    set(mkl-static-osx-64-md5 4a099581ba95cc50bb538598b26389e4)
    set(mkl-static-win-64-md5 69aef10428893314bc486e81397e1b25)
    set(mkl-devel-linux-64-md5 2432ad963e3f7e4619ffc7f896178fbe)
    set(mkl-devel-osx-64-md5 61b84a60715a3855a2097a3b619a00c8)
    set(mkl-devel-win-64-md5 6128dee67d2b20ff534cf54757f623e0)

    # To compute file names, we use the following bash script:
    # for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-file") <(conda search --override-channel --channel intel $f=2021.3.0 --platform $os-64 -i | grep file | cut -d : -f 2); done; done
    set(mkl-linux-64-file mkl-2021.3.0-intel_520.tar.bz2)
    set(mkl-osx-64-file mkl-2021.3.0-intel_517.tar.bz2)
    set(mkl-win-64-file mkl-2021.3.0-intel_524.tar.bz2)
    set(mkl-include-linux-64-file mkl-include-2021.3.0-intel_520.tar.bz2)
    set(mkl-include-osx-64-file mkl-include-2021.3.0-intel_517.tar.bz2)
    set(mkl-include-win-64-file mkl-include-2021.3.0-intel_524.tar.bz2)
    set(mkl-static-linux-64-file mkl-static-2021.3.0-intel_520.tar.bz2)
    set(mkl-static-osx-64-file mkl-static-2021.3.0-intel_517.tar.bz2)
    set(mkl-static-win-64-file mkl-static-2021.3.0-intel_524.tar.bz2)
    set(mkl-devel-linux-64-file mkl-devel-2021.3.0-intel_520.tar.bz2)
    set(mkl-devel-osx-64-file mkl-devel-2021.3.0-intel_517.tar.bz2)
    set(mkl-devel-win-64-file mkl-devel-2021.3.0-intel_524.tar.bz2)
endif()

# On Windows, `mkl-devel` contains the .lib files (needed at link time),
# while `mkl` contains the .dll files (needed at run time). On macOS/Linux,
# `mkl-devel` is empty, and only `mkl` is needed for the .so/.dylib
if(MKL_LINKING STREQUAL static)
    set(MKL_REMOTES mkl-include mkl-static)
else()
    set(MKL_REMOTES mkl-include mkl mkl-devel)
endif()

include(CPM)
foreach(name IN ITEMS ${MKL_REMOTES})
    CPMAddPackage(
        NAME ${name}
        URL https://anaconda.org/intel/${name}/${MKL_VERSION}/download/${MKL_PLATFORM}/${${name}-${MKL_PLATFORM}-file}
        URL_MD5 ${${name}-${MKL_PLATFORM}-md5}
    )
endforeach()

################################################################################

# Find compiled libraries
unset(MKL_LIB_HINTS)
unset(MKL_LIB_SUFFIX)
unset(MKL_DLL_SUFFIX)
if(MKL_LINKING STREQUAL static)
    list(APPEND MKL_LIB_HINTS
        "${mkl-static_SOURCE_DIR}/lib"
        "${mkl-static_SOURCE_DIR}/Library/bin"
        "${mkl-static_SOURCE_DIR}/Library/lib"
    )
else()
    if(WIN32)
        set(MKL_LIB_SUFFIX "_dll")
    endif()
    if(WIN32 AND MKL_VERSION STREQUAL 2021.3.0)
        set(MKL_DLL_SUFFIX ".1")
    endif()
    list(APPEND MKL_LIB_HINTS
        "${mkl_SOURCE_DIR}/lib"
        "${mkl_SOURCE_DIR}/Library/bin"
        "${mkl-devel_SOURCE_DIR}/Library/lib"
    )
endif()

# Note: Since CMake 3.17, find_library accepts a REQUIRED arg that stops processing
# if the file is not found, thus removing the need for this assert() function.
function(mkl_assert_is_found var)
    if(NOT ${var})
        message(FATAL_ERROR "Could not find " ${var})
    endif()
endfunction()

# Creates one IMPORTED target for each requested library
function(mkl_add_imported_library name)
    # Parse optional arguments
    set(options NO_DLL)
    set(one_value_args "")
    set(multi_value_args "")
    cmake_parse_arguments(PARSE_ARGV 1 ARGS "${options}" "${one_value_args}" "${multi_value_args}")

    # Define type of imported library
    if(MKL_LINKING STREQUAL static)
        set(MKL_TYPE STATIC)
    elseif(ARGS_NO_DLL)
        set(MKL_TYPE STATIC)
    else()
        set(MKL_TYPE SHARED)
    endif()

    # Create imported target for library
    add_library(mkl::${name} ${MKL_TYPE} IMPORTED GLOBAL)
    set_target_properties(mkl::${name} PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES CXX)

    # Important to avoid absolute path referencing of the libraries in the executables
    set_target_properties(mkl::${name} PROPERTIES IMPORTED_NO_SONAME TRUE)

    # Find library file
    string(TOUPPER mkl_${name}_library LIBVAR)
    find_library(${LIBVAR}
        NAMES mkl_${name}${MKL_LIB_SUFFIX}
        HINTS ${MKL_LIB_HINTS}
        NO_DEFAULT_PATH
    )
    mkl_assert_is_found(${LIBVAR})
    message(STATUS "Creating target mkl::${name} for lib file: ${${LIBVAR}}")

    # Set imported location
    if(WIN32 AND NOT (MKL_LINKING STREQUAL static))
        # On Windows, when linking against shared libraries, we set IMPORTED_IMPLIB to the .lib file
        if(ARGS_NO_DLL)
            # Interface library only, no .dll
            set_target_properties(mkl::${name} PROPERTIES
                IMPORTED_LOCATION "${${LIBVAR}}"
            )
        else()
            # Find dll file and set IMPORTED_LOCATION to the .dll file
            string(TOUPPER mkl_${name}_dll DLLVAR)
            find_file(${DLLVAR}
                NAMES mkl_${name}${MKL_DLL_SUFFIX}.dll
                HINTS ${MKL_LIB_HINTS}
                NO_DEFAULT_PATH
            )
            mkl_assert_is_found(${DLLVAR})
            message(STATUS "Creating target mkl::${name} for dll file: ${${DLLVAR}}")
            set_target_properties(mkl::${name} PROPERTIES
                IMPORTED_IMPLIB "${${LIBVAR}}"
                IMPORTED_LOCATION "${${DLLVAR}}"
            )
        endif()
    else()
        # Otherwise, we ignore the dll location, and simply set main library location
        set_target_properties(mkl::${name} PROPERTIES
            IMPORTED_LOCATION "${${LIBVAR}}"
        )
    endif()

    # Add mkl::core as a dependency
    set(mkl_root_libs core rt)
    if(NOT name IN_LIST mkl_root_libs)
        target_link_libraries(mkl::${name} INTERFACE mkl::core)
    endif()

    # Add as dependency to the meta target mkl::mkl
    target_link_libraries(mkl::mkl INTERFACE mkl::${name})
endfunction()

# On Windows, creates an IMPORTED target for each given .dll
function(mkl_add_shared_libraries)
    if(NOT WIN32 OR MKL_LINKING STREQUAL static)
        return()
    endif()

    foreach(name IN ITEMS ${ARGN})
        # Create imported target for library. Here, we use MODULE, since those are not linked into
        # other targets, but rather loaded at runtime using dlopen-like functionality
        add_library(mkl::${name} MODULE IMPORTED GLOBAL)
        set_target_properties(mkl::${name} PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES CXX)

        # Find dll file
        string(TOUPPER mkl_${name}_dll DLLVAR)
        find_file(${DLLVAR}
            NAMES mkl_${name}${MKL_DLL_SUFFIX}.dll
            HINTS ${MKL_LIB_HINTS}
            NO_DEFAULT_PATH
        )
        mkl_assert_is_found(${DLLVAR})
        message(STATUS "Creating target mkl::${name} for dll file: ${${DLLVAR}}")
        set_target_properties(mkl::${name} PROPERTIES
            IMPORTED_LOCATION "${${DLLVAR}}"
        )

        # Set as dependency to the meta target mkl::mkl. We cannot directly use `target_link_libraries`, since a MODULE
        # target represents a runtime dependency and cannot be linked against. Instead, we populate a custom property
        # mkl_RUNTIME_DEPENDENCIES, which we use to manually copy dlls into an executable folder.
        #
        # Note that we have to use a custom property name starting with [a-z_] (I assume uppercase is reserved for
        # CMake). See comment https://gitlab.kitware.com/cmake/cmake/-/issues/19145#note_706678
        #
        # See also this thread for explicit tracking of runtime requirements:
        # https://gitlab.kitware.com/cmake/cmake/-/issues/20417
        set_property(TARGET mkl::mkl PROPERTY mkl_RUNTIME_DEPENDENCIES mkl::${name} APPEND)
    endforeach()
endfunction()

# Set cyclic dependencies between mkl targets. CMake handles cyclic dependencies between static libraries by repeating
# them on the linker line. See [1] for more details. Note that since our targets are IMPORTED, we have to set the
# property directly rather than rely on target_link_libraries().
#
# [1]: https://cmake.org/cmake/help/latest/command/target_link_libraries.html#cyclic-dependencies-of-static-libraries
function(mkl_set_dependencies)
    # We create a strongly connected dependency graph between mkl imported static libraries. Then, we use
    # IMPORTED_LINK_INTERFACE_MULTIPLICITY to tell CMake how many time each item in the connected component needs to be
    # repeated on the command-line. See relevant CMake issue:
    # https://gitlab.kitware.com/cmake/cmake/-/issues/17964
    #
    # Note: For some reason, creating a fully connected graph between all the list items would lead CMake to *not*
    # repeat the first item in the component. I had to create a proper cordless cycle to make this work.
    if(MKL_LINKING STREQUAL static)
        set(args_shifted ${ARGN})
        list(POP_FRONT args_shifted first_item)
        list(APPEND args_shifted ${first_item})
        foreach(a b IN ZIP_LISTS ARGN args_shifted)
            set_target_properties(mkl::${a} PROPERTIES INTERFACE_LINK_LIBRARIES mkl::${b})
        endforeach()

        # Manually increase until all cyclic dependencies are resolved.
        set_property(TARGET mkl::${first_item} PROPERTY IMPORTED_LINK_INTERFACE_MULTIPLICITY 4)
    endif()
endfunction()

################################################################################


# Create a proper imported target for MKL
add_library(mkl::mkl INTERFACE IMPORTED GLOBAL)

# Find header directory
find_path(MKL_INCLUDE_DIR
    NAMES mkl.h
    HINTS
        ${mkl-include_SOURCE_DIR}/include
        ${mkl-include_SOURCE_DIR}/Library/include
    NO_DEFAULT_PATH
)
mkl_assert_is_found(MKL_INCLUDE_DIR)
message(STATUS "MKL include dir: ${MKL_INCLUDE_DIR}")
target_include_directories(mkl::mkl INTERFACE ${MKL_INCLUDE_DIR})

if(MKL_LINKING STREQUAL sdl)
    # Link against a single dynamic lib
    mkl_add_imported_library(rt)
else()
    # Link against each component explicitly

    # Computational library
    mkl_add_imported_library(core)

    # Interface library
    if(WIN32)
        mkl_add_imported_library(intel_${MKL_INTERFACE} NO_DLL)
    else()
        mkl_add_imported_library(intel_${MKL_INTERFACE})
    endif()

    # Thread library
    if(MKL_THREADING STREQUAL sequential)
        mkl_add_imported_library(sequential)
        mkl_set_dependencies(core intel_${MKL_INTERFACE} sequential)
    else()
        mkl_add_imported_library(tbb_thread)
        mkl_set_dependencies(core intel_${MKL_INTERFACE} tbb_thread)
    endif()

    # Instructions sets specific libraries

    # Kernel - Required
    mkl_add_shared_libraries(def)

    # Kernel - Optional
    mkl_add_shared_libraries(avx avx2 avx512 mc mc3)

    # Vector Mathematics - Required
    mkl_add_shared_libraries(vml_def vml_cmpt)

    # Vector Mathematics - Optional
    mkl_add_shared_libraries(vml_avx vml_avx2 vml_avx512 vml_mc vml_mc2 vml_mc3)
endif()

# Compile definitions.
if(MKL_INTERFACE STREQUAL "ilp64")
    target_compile_definitions(mkl::mkl INTERFACE MKL_ILP64)
endif()

# Also requires the math system library (-lm)?
if(NOT MSVC)
    find_library(LIBM_LIBRARY m)
    mkl_assert_is_found(LIBM_LIBRARY)
    target_link_libraries(mkl::mkl INTERFACE ${LIBM_LIBRARY})
endif()

# If using TBB, we need to specify the dependency
if(MKL_THREADING STREQUAL "tbb")
    include(tbb)
    target_link_libraries(mkl::tbb_thread INTERFACE TBB::tbb)
endif()
