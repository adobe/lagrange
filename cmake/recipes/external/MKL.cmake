#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET MKL::MKL)
    return()
endif()

message(STATUS "Third-party (external): creating target 'MKL::MKL'")

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
set(MKL_VERSION "2024.2.1" CACHE STRING "MKL version to use (2024.2.1)")
set(MKL_VERSION_CHOICES 2024.2.1)
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

# Check option for the library version
if(NOT MKL_VERSION IN_LIST MKL_VERSION_CHOICES)
    message(FATAL_ERROR "Unrecognized option for MKL_VERSION: ${MKL_VERSION}")
endif()

################################################################################

if(WIN32)
    set(MKL_PLATFORM win-64)
    set(MKL_PLATFORM_TAG win_amd64)
elseif(APPLE)
    message(FATAL_ERROR "MKL not supported on macOS")
elseif(UNIX)
    set(MKL_PLATFORM linux-64)
    set(MKL_PLATFORM_TAG manylinux1_x86_64)
endif()

#
# How to get URL info:
# https://pypi.org/pypi/mkl/2024.2.1/json
#
# URL format:
# https://files.pythonhosted.org/packages/${digest_a}/${digest_b}/${digest_c}/${name}-${version}-${python_tag}-${abi_tag}-${platform_tag}.whl
#
# For now we just extract the blake2b_256 hashes manually.
#
if(MKL_VERSION VERSION_EQUAL 2024.2.1)
    set(mkl-linux-64-md5 95ae2d0f0ac0f0d75b5e6eebfe950a61)
    set(mkl-win-64-md5 e00cd0a87e5b4eb30abf963fcff35c4a)
    set(mkl-include-linux-64-md5 cd093868db92753f621cbc3b32a551c7)
    set(mkl-include-win-64-md5 d389bf244e188613090b5e4fb1dff1b0)
    set(mkl-static-linux-64-md5 2a30a478886c6baaf4c608005f9bb504)
    set(mkl-static-win-64-md5 5c09cba82702c538184b44d3969062e8)
    set(mkl-devel-linux-64-md5 e2952a0647f5a8119a90ce5e546614b9)
    set(mkl-devel-win-64-md5 616e14b105c8c47aa81f23f7eafa320b)

    set(mkl-linux-64-blake2b d817d88e4a21f06ea24b2dc0b01658f31bee229daad07f8d4850ba398ddf6235)
    set(mkl-win-64-blake2b 7c2695144302ad2df906529b875f66d3e210bef0b7df7b114a85a7d797bc2b40)
    set(mkl-include-linux-64-blake2b 9135878af3209787537f1e93e721cacc9b2a72230cf579854e65a7a920e5de33)
    set(mkl-include-win-64-blake2b aeb7e4ff13142b932fefe698d14a0bf04461f617093ef30a74ec64cb41955ebb)
    set(mkl-static-linux-64-blake2b d2d6903974cae7612e2c7ed42d3ccbe65cf0cbbf2130e5a882bb420308c2cba0)
    set(mkl-static-win-64-blake2b 443aa2861e049e56ab09be99567dc993474fc15a315338d71371242170122f27)
    set(mkl-devel-linux-64-blake2b 27000e05a83be406a744cb3591496d3dab8b4d7d00ea18ffccda9cfcc36b9cc7)
    set(mkl-devel-win-64-blake2b 5f27d3be9e360c1e31f899e9ae80d4d5f32293311f66f0c17e2e52faf8430b26)
else()
    message(FATAL_ERROR "MKL version not supported")
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
    set(digest_full ${${name}-${MKL_PLATFORM}-blake2b})
    string(SUBSTRING ${digest_full} 0 2 digest_a)
    string(SUBSTRING ${digest_full} 2 2 digest_b)
    string(SUBSTRING ${digest_full} 4 -1 digest_c)
    set(version ${MKL_VERSION})
    set(python_tag py2.py3)
    set(abi_tag none)
    set(platform_tag ${MKL_PLATFORM_TAG})
    string(REPLACE "-" "_" pkg_name ${name})
    CPMAddPackage(
        NAME ${name}
        URL "https://files.pythonhosted.org/packages/${digest_a}/${digest_b}/${digest_c}/${pkg_name}-${version}-${python_tag}-${abi_tag}-${platform_tag}.whl"
        URL_MD5 ${${name}-${MKL_PLATFORM}-md5}
    )
endforeach()

################################################################################

# Find compiled libraries
unset(MKL_LIB_HINTS)
unset(MKL_LIB_SUFFIX)
unset(MKL_DLL_SUFFIX)
if(MKL_LINKING STREQUAL static)
    file(GLOB MKL_LIB_HINTS LIST_DIRECTORIES true "${mkl-static_SOURCE_DIR}/*.data")
    list(APPEND MKL_LIB_HINTS "${mkl-static_SOURCE_DIR}")
else()
    if(WIN32)
        set(MKL_LIB_SUFFIX "_dll")
    endif()
    if(MKL_VERSION STREQUAL 2024.2.1)
        set(MKL_DLL_SUFFIX ".2")
    endif()
    file(GLOB MKL_LIB_HINTS LIST_DIRECTORIES true "${mkl_SOURCE_DIR}/*.data" "${mkl-devel_SOURCE_DIR}/*.data")
    list(APPEND MKL_LIB_HINTS "${mkl_SOURCE_DIR}" "${mkl-devel_SOURCE_DIR}")
endif()
set(MKL_LIB_PATH_SUFFIXES
    lib
    Library/bin
    Library/lib
    data/Library/bin
    data/Library/lib
    data/lib
    data/bin
)

# Soname shenanigans for macOS and Linux
set(MKL_SONAME_PREFIX)
if(APPLE)
    set(MKL_SONAME_PREFIX "@rpath/")
endif()

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
    add_library(MKL::${name} ${MKL_TYPE} IMPORTED GLOBAL)
    set_target_properties(MKL::${name} PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES CXX)

    # Find library file
    string(TOUPPER mkl_${name}_library LIBVAR)
    # A bit hacky but MKL libs are weird...
    set(OLD_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    if(LINUX)
        set(CMAKE_FIND_LIBRARY_SUFFIXES "")
        set(mkl_search_name mkl_${name}${MKL_LIB_SUFFIX}.so${MKL_DLL_SUFFIX})
    else()
        set(mkl_search_name mkl_${name}${MKL_LIB_SUFFIX})
    endif()
    find_library(${LIBVAR}
        NAMES ${mkl_search_name}
        HINTS ${MKL_LIB_HINTS}
        PATH_SUFFIXES ${MKL_LIB_PATH_SUFFIXES}
        NO_DEFAULT_PATH
        REQUIRED
    )
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${OLD_CMAKE_FIND_LIBRARY_SUFFIXES})
    message(STATUS "Creating target MKL::${name} for lib file: ${${LIBVAR}}")

    # Set imported location
    if(WIN32 AND NOT (MKL_LINKING STREQUAL static))
        # On Windows, when linking against shared libraries, we set IMPORTED_IMPLIB to the .lib file
        if(ARGS_NO_DLL)
            # Interface library only, no .dll
            set_target_properties(MKL::${name} PROPERTIES
                IMPORTED_LOCATION "${${LIBVAR}}"
            )
        else()
            # Find dll file and set IMPORTED_LOCATION to the .dll file
            string(TOUPPER mkl_${name}_dll DLLVAR)
            message("LIB_HINTS: ${MKL_LIB_HINTS}")
            message("LIB_PATH_SUFFIXES: ${MKL_LIB_PATH_SUFFIXES}")
            find_file(${DLLVAR}
                NAMES mkl_${name}${MKL_DLL_SUFFIX}.dll
                HINTS ${MKL_LIB_HINTS}
                PATH_SUFFIXES ${MKL_LIB_PATH_SUFFIXES}
                NO_DEFAULT_PATH
                REQUIRED
            )
            message(STATUS "Creating target MKL::${name} for dll file: ${${DLLVAR}}")
            set_target_properties(MKL::${name} PROPERTIES
                IMPORTED_IMPLIB "${${LIBVAR}}"
                IMPORTED_LOCATION "${${DLLVAR}}"
            )
        endif()
    else()
        # Otherwise, we ignore the dll location, and simply set main library location
        if(UNIX)
            file(REAL_PATH "${${LIBVAR}}" mkl_lib_path)
            cmake_path(GET ${LIBVAR} FILENAME mkl_lib_filename)
            # On Linux, we need to resolve symlinks to the actual library file, and set the SONAME
            # to the unversioned file name. If we don't, CMake's install() step will not the
            # versioned filename, causing linking errors in our Python package.
            set_target_properties(MKL::${name} PROPERTIES
                IMPORTED_SONAME "${MKL_SONAME_PREFIX}${mkl_lib_filename}"
                IMPORTED_LOCATION "${mkl_lib_path}"
            )
        else()
            set_target_properties(MKL::${name} PROPERTIES
                IMPORTED_LOCATION "${${LIBVAR}}"
            )
        endif()
    endif()

    # Add MKL::core as a dependency
    set(mkl_root_libs core rt)
    if(NOT name IN_LIST mkl_root_libs)
        target_link_libraries(MKL::${name} INTERFACE MKL::core)
    endif()

    # Add as dependency to the meta target MKL::MKL
    target_link_libraries(MKL::MKL INTERFACE MKL::${name})
endfunction()

# On Windows, creates an IMPORTED target for each given .dll
# On macOS/Linux, we still need this to correctly install runtime dependencies
function(mkl_add_shared_libraries)
    if(MKL_LINKING STREQUAL static)
        return()
    endif()

    # On macOS, some of these libs are not present, so we mark them as optional
    set(optional_libs)
    if(APPLE)
        list(APPEND optional_libs def mc vml_def vml_mc)
    endif()

    foreach(name IN ITEMS ${ARGN})
        if(name IN_LIST optional_libs)
            set(required_flag)
        else()
            set(required_flag REQUIRED)
        endif()

        if(WIN32)
            # Find dll file
            string(TOUPPER mkl_${name}_dll DLLVAR)
            find_file(${DLLVAR}
                NAMES mkl_${name}${MKL_DLL_SUFFIX}.dll
                HINTS ${MKL_LIB_HINTS}
                PATH_SUFFIXES ${MKL_LIB_PATH_SUFFIXES}
                NO_DEFAULT_PATH
            )
        else()
            string(TOUPPER mkl_${name} DLLVAR)
            # A bit hacky but MKL libs are weird...
            set(OLD_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
            if(LINUX)
                set(CMAKE_FIND_LIBRARY_SUFFIXES "")
                set(mkl_search_name mkl_${name}${MKL_LIB_SUFFIX}.so${MKL_DLL_SUFFIX})
            else()
                set(mkl_search_name mkl_${name}${MKL_LIB_SUFFIX}${MKL_DLL_SUFFIX})
            endif()
            message("MKL_LIB_HINTS: ${MKL_LIB_HINTS}")
            message("MKL_LIB_PATH_SUFFIXES: ${MKL_LIB_PATH_SUFFIXES}")
            message("mkl_search_name: ${mkl_search_name}")
            find_library(${DLLVAR}
                NAMES ${mkl_search_name}
                HINTS ${MKL_LIB_HINTS}
                PATH_SUFFIXES ${MKL_LIB_PATH_SUFFIXES}
                NO_DEFAULT_PATH
                ${required_flag}
            )
            set(CMAKE_FIND_LIBRARY_SUFFIXES ${OLD_CMAKE_FIND_LIBRARY_SUFFIXES})
        endif()

        if(NOT ${DLLVAR})
            message(STATUS "Skipping mkl_${name}${MKL_LIB_SUFFIX}")
            return()
        endif()

        # Create imported target for library. Here, we use MODULE, since those are not linked into
        # other targets, but rather loaded at runtime using dlopen-like functionality
        add_library(MKL::${name} MODULE IMPORTED GLOBAL)
        set_target_properties(MKL::${name} PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES CXX)

        message(STATUS "Creating target MKL::${name} for shared lib file: ${${DLLVAR}}")
        set_target_properties(MKL::${name} PROPERTIES
            IMPORTED_LOCATION "${${DLLVAR}}"
        )
        if(UNIX)
            get_filename_component(mkl_lib_filename "${${LIBVAR}}" NAME)
            set_target_properties(MKL::${name} PROPERTIES
                IMPORTED_SONAME "${MKL_SONAME_PREFIX}${mkl_lib_filename}"
            )
        endif()

        # Set as dependency to the meta target MKL::MKL. We cannot directly use `target_link_libraries`, since a MODULE
        # target represents a runtime dependency and cannot be linked against. Instead, we populate a custom property
        # mkl_RUNTIME_DEPENDENCIES, which we use to manually copy dlls into an executable folder.
        #
        # Note that we have to use a custom property name starting with [a-z_] (I assume uppercase is reserved for
        # CMake). See comment https://gitlab.kitware.com/cmake/cmake/-/issues/19145#note_706678
        #
        # See also this thread for explicit tracking of runtime requirements:
        # https://gitlab.kitware.com/cmake/cmake/-/issues/20417
        set_property(TARGET MKL::MKL PROPERTY mkl_RUNTIME_DEPENDENCIES MKL::${name} APPEND)
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
            set_target_properties(MKL::${a} PROPERTIES INTERFACE_LINK_LIBRARIES MKL::${b})
        endforeach()

        # Manually increase until all cyclic dependencies are resolved.
        set_property(TARGET MKL::${first_item} PROPERTY IMPORTED_LINK_INTERFACE_MULTIPLICITY 4)
    endif()
endfunction()

################################################################################

# Create a proper imported target for MKL
add_library(MKL::MKL INTERFACE IMPORTED GLOBAL)

# Find header directory
file(GLOB MKL_INCLUDE_HINTS LIST_DIRECTORIES true "${mkl-include_SOURCE_DIR}/*.data")
message("MKL_INCLUDE_HINTS: ${MKL_INCLUDE_HINTS}")
find_path(MKL_INCLUDE_DIR
    NAMES mkl.h
    HINTS
        ${mkl-include_SOURCE_DIR}/
        ${mkl-include_SOURCE_DIR}/Library
        ${mkl-include_SOURCE_DIR}/data/Library
        ${MKL_INCLUDE_HINTS}
        ${MKL_INCLUDE_HINTS}/Library
        ${MKL_INCLUDE_HINTS}/data/Library
        ${MKL_INCLUDE_HINTS}/data
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    REQUIRED
)
message(STATUS "MKL include dir: ${MKL_INCLUDE_DIR}")
target_include_directories(MKL::MKL INTERFACE ${MKL_INCLUDE_DIR})

if(MKL_LINKING STREQUAL sdl)
    # Link against a single dynamic lib
    mkl_add_imported_library(rt)

    # Add every other library as a runtime dependency (to ensure proper installation)

    # Computational library
    mkl_add_shared_libraries(core)

    # Interface library
    if(WIN32)
        mkl_add_shared_libraries(intel_${MKL_INTERFACE} NO_DLL)
    else()
        mkl_add_shared_libraries(intel_${MKL_INTERFACE})
    endif()

    # Thread library
    if(MKL_THREADING STREQUAL sequential)
        mkl_add_shared_libraries(sequential)
    else()
        mkl_add_shared_libraries(tbb_thread)
    endif()
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
endif()

# Instructions sets specific libraries

# Kernel - Required
mkl_add_shared_libraries(def)

# Kernel - Optional
mkl_add_shared_libraries(avx2 avx512 mc3)

# Vector Mathematics - Required
mkl_add_shared_libraries(vml_def vml_cmpt)

# Vector Mathematics - Optional
mkl_add_shared_libraries(vml_avx2 vml_avx512 vml_mc3)

# Compile definitions.
if(MKL_INTERFACE STREQUAL "ilp64")
    target_compile_definitions(MKL::MKL INTERFACE MKL_ILP64)
endif()

# Also requires the math system library (-lm)?
if(NOT MSVC)
    find_library(LIBM_LIBRARY m REQUIRED)
    target_link_libraries(MKL::MKL INTERFACE ${LIBM_LIBRARY})
endif()

# If using TBB, we need to specify the dependency
if(MKL_THREADING STREQUAL "tbb")
    include(tbb)
    if(TARGET MKL::tbb_thread)
        target_link_libraries(MKL::tbb_thread INTERFACE TBB::tbb)
    endif()
    if(TARGET MKL::rt)
        target_link_libraries(MKL::rt INTERFACE TBB::tbb)
    endif()
endif()
