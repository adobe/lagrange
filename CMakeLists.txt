#
# Copyright 2016 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

# Detects whether this is a top-level project
get_directory_property(HAS_PARENT PARENT_DIRECTORY)
if(HAS_PARENT)
    set(LAGRANGE_TOPLEVEL_PROJECT OFF)
else()
    set(LAGRANGE_TOPLEVEL_PROJECT ON)
endif()

# Check required CMake version
set(REQUIRED_CMAKE_VERSION "3.24.0")
if(LAGRANGE_TOPLEVEL_PROJECT)
    cmake_minimum_required(VERSION ${REQUIRED_CMAKE_VERSION})
else()
    # Don't use cmake_minimum_required here to avoid implicitly overriding parent policies
    if(${CMAKE_VERSION} VERSION_LESS ${REQUIRED_CMAKE_VERSION})
        message(FATAL_ERROR "CMake required version to build Lagrange is ${REQUIRED_CMAKE_VERSION}")
    endif()
endif()

cmake_policy(SET CMP0054 NEW) # Only interpret if() arguments as variables or keywords when unquoted.
cmake_policy(SET CMP0076 NEW) # target_sources() command converts relative paths to absolute.
cmake_policy(SET CMP0141 NEW) # MSVC debug information format flags are selected by an abstraction.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW) # MSVC runtime library flags are selected by an abstraction.
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW) # Set the timestamps of all extracted contents to the time of the extraction.

# Include user-provided default options if available. We do that before the main
# `project()` so that we can define the C/C++ compilers from the option file.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/LagrangeOptions.cmake)
    if(LAGRANGE_TOPLEVEL_PROJECT)
        message(STATUS "Using local options file: ${CMAKE_CURRENT_SOURCE_DIR}/LagrangeOptions.cmake")
        include(${CMAKE_CURRENT_SOURCE_DIR}/LagrangeOptions.cmake)
    else()
        message(WARNING "Ignoring local options file for non-top-level build: "
                        "${CMAKE_CURRENT_SOURCE_DIR}/LagrangeOptions.cmake")
    endif()
endif()

# Enable ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    option(LAGRANGE_WITH_CCACHE "Enable ccache when building Lagrange" ${LAGRANGE_TOPLEVEL_PROJECT})
else()
    option(LAGRANGE_WITH_CCACHE "Enable ccache when building Lagrange" OFF)
endif()
if(LAGRANGE_WITH_CCACHE AND CCACHE_PROGRAM)
    set(ccacheEnv
        CCACHE_BASEDIR=${CMAKE_BINARY_DIR}
        CCACHE_SLOPPINESS=clang_index_store,include_file_ctime,include_file_mtime,locale,pch_defines,time_macros
    )
    foreach(lang IN ITEMS C CXX)
        set(CMAKE_${lang}_COMPILER_LAUNCHER
            ${CMAKE_COMMAND} -E env ${ccacheEnv} ${CCACHE_PROGRAM}
        )
    endforeach()
endif()

# Set default macOS deployment target
if(LAGRANGE_TOPLEVEL_PROJECT)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif()

################################################################################
file(READ "VERSION" lagrange_version)
string(STRIP ${lagrange_version} lagrange_version)
project(Lagrange VERSION ${lagrange_version})

# Detects whether internal libs are available
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/cmake/recipes/internal")
    set(LAGRANGE_NO_INTERNAL OFF)
else()
    set(LAGRANGE_NO_INTERNAL ON)
endif()

# Define this first since other options defaults depend on this
option(LAGRANGE_JENKINS "Enable specific configs when building on Jenkins" OFF)

# Whether to enable compilation tests by default
if(WIN32 AND NOT LAGRANGE_JENKINS)
    set(LAGRANGE_COMPILE_TESTS_DEFAULT NO)
else()
    set(LAGRANGE_COMPILE_TESTS_DEFAULT ${LAGRANGE_TOPLEVEL_PROJECT})
endif()

# Meta target: ALL includes all optional modules and UI.
option(LAGRANGE_ALL                "Build all lagrange modules" OFF)

# General CMake options
option(LAGRANGE_ASSERT_DEBUG_BREAK      "Assert will break into debugger on failure"           ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_COMPILE_TESTS           "Enable compilation tests"                             ${LAGRANGE_COMPILE_TESTS_DEFAULT})
option(LAGRANGE_COPY_RUNTIME_DEPS       "Copy runtime dependencies into executable folders"    ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_DISABLE_FPE             "Disable floating point exception code"                OFF)
option(LAGRANGE_DOCUMENTATION           "Build Doxygen documentation"                          OFF)
option(LAGRANGE_ENABLE_GPU_TESTS        "Enable unit tests that run on the GPU"                ON)
option(LAGRANGE_ENABLE_LEGACY_FUNCTIONS "Enable support for legacy functions."                 ON)
option(LAGRANGE_EXAMPLES                "Build all examples"                                   ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_INSTALL                 "Enable installation"                                  ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_LIMIT_PARALLELISM       "Limit parallelism according to available cpu/memory"  OFF)
option(LAGRANGE_MORE_WARNINGS           "Increase the level of warnings when compiling"        OFF)
option(LAGRANGE_PERFORMANCE_TESTS       "Build all performance tests"                          OFF)
option(LAGRANGE_UNIT_TESTS              "Build all unit tests"                                 ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_USE_PCH                 "Enable precompiled headers"                           OFF)
option(LAGRANGE_USE_WASM_EXCEPTIONS     "Use -fwasm-exception flag with Emscripten"            OFF)
option(LAGRANGE_WITH_ONETBB             "Build Lagrange with OneTBB 2021 rather than TBB 2020" ON)
option(LAGRANGE_WITH_TRACY              "Build tracy client with Lagrange"                     OFF)

# Specific override for building on Jenkins
if(LAGRANGE_JENKINS)
    # IPO is very slow and not needed for CI purposes
    option(TBB_ENABLE_IPO "Enable Interprocedural Optimization (IPO) during the compilation" OFF)
endif()

# When building Arpack from source, we need to enable Fortran support in the top-level directory
if(ARPACK_BUILD_FROM_SOURCE)
    enable_language(Fortran)
endif()

# Allowlist/blocklist restricting which modules are allowed in the sub-project. This is an advanced
# option, targeted at users who wish to have explicit knowledge of which module is implicitly
# compiled by Lagrange. By default, an empty list means all modules are allowed.
set(LAGRANGE_ALLOWLIST "" CACHE STRING "List of modules allowed in the project (empty: allow everything)")
set(LAGRANGE_BLOCKLIST "" CACHE STRING "List of modules excluded from the project")

# Third-party dependency management
option(LAGRANGE_EXTERNAL_FIRST    "Third-party libs: prefer public mirrors over corporate ones" OFF)
option(LAGRANGE_EXTERNAL_ONLY     "Third-party libs: only download from public mirrors"         ${LAGRANGE_NO_INTERNAL})

# Artifactory key file
set(LAGRANGE_ARTIFACTORY_KEYFILE "" CACHE FILEPATH "Path to secret artifactory key.")

# Website repository (extract & check code)
set(LAGRANGE_WEBSITE_REPO "" CACHE FILEPATH "Path to the website repository for markdown code extraction")

# Set default minimum C++ standard
if(LAGRANGE_TOPLEVEL_PROJECT)
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard version to compile with")
    set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Whether the C++ standard version is a requirement")
    set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Whether compiler specific extensions are requested")
endif()

if(LAGRANGE_EXTERNAL_ONLY)
    list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/recipes/external/")
elseif(LAGRANGE_EXTERNAL_FIRST)
    list(PREPEND CMAKE_MODULE_PATH
        "${PROJECT_SOURCE_DIR}/cmake/recipes/external/"
        "${PROJECT_SOURCE_DIR}/cmake/recipes/internal/"
    )
else()
    list(PREPEND CMAKE_MODULE_PATH
        "${PROJECT_SOURCE_DIR}/cmake/recipes/internal/"
        "${PROJECT_SOURCE_DIR}/cmake/recipes/external/"
    )
endif()
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/lagrange/")

# Lagrange folder prefix for IDE
if(LAGRANGE_TOPLEVEL_PROJECT)
    set(LAGRANGE_IDE_PREFIX_DEFAULT "")
else()
    set(LAGRANGE_IDE_PREFIX_DEFAULT "third_party/")
endif()
set(LAGRANGE_IDE_PREFIX ${LAGRANGE_IDE_PREFIX_DEFAULT} CACHE STRING "Folder prefix for Lagrange targets in MSVC/Xcode")

# When building python module, compile MKL/TBB as a shared library
if(LAGRANGE_MODULE_PYTHON OR LAGRANGE_ALL)
    set(MKL_LINKING "dynamic" CACHE STRING "Linking strategy to use with MKL (static, dynamic or sdl)")
    option(TBB_PREFER_STATIC    "Build with static TBB"     OFF)
endif()

# When building anorigami module, we also need to compile tbbmalloc
if(LAGRANGE_MODULE_ANORIGAMI OR (LAGRANGE_ALL AND NOT LAGRANGE_NO_INTERNAL))
    option(TBB_BUILD_TBBMALLOC  "Build TBB malloc library"  ON)
endif()

# On Linux & Windows we use MKL to provide BLAS/LAPACK. Since it comes precompiled with /MD on Windows,
# we need to use the MSVC runtime library flag globally for the whole project.
file(READ "cmake/lagrange/lagrangeMklModules.txt" LAGRANGE_MKL_MODULES)
if(LAGRANGE_TOPLEVEL_PROJECT)
    if(LAGRANGE_ALL AND NOT LAGRANGE_NO_INTERNAL)
        # Set MSVC runtime library globally for all targets
        message(STATUS "Forcing /MD globally due to LAGRANGE_ALL requiring MKL")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "Select the MSVC runtime library")
    else()
        foreach(mkl_module_lc IN ITEMS ${LAGRANGE_MKL_MODULES})
            string(TOUPPER ${mkl_module_lc} mkl_module_uc)
            if(LAGRANGE_MODULE_${mkl_module_uc})
                # Set MSVC runtime library globally for all targets
                message(STATUS "Forcing /MD globally due to lagrange::${mkl_module_lc} requiring MKL")
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "Select the MSVC runtime library")
            endif()
        endforeach()
    endif()
endif()

# Register project source/binary dir in global properties, so they can be
# retrieved from Lagrange CMake functions when called by client code.
set_property(GLOBAL PROPERTY __lagrange_source_dir ${PROJECT_SOURCE_DIR})
set_property(GLOBAL PROPERTY __lagrange_binary_dir ${PROJECT_BINARY_DIR})
set_property(GLOBAL PROPERTY __lagrange_module_path ${CMAKE_MODULE_PATH})

include(lagrange_add_compile_test)
include(lagrange_add_example)
include(lagrange_add_executable)
include(lagrange_add_module)
include(lagrange_add_performance)
include(lagrange_add_python_binding)
include(lagrange_add_test)
include(lagrange_download_data)
include(lagrange_global_flags)
include(lagrange_has_onetbb)
include(lagrange_include_modules)
include(lagrange_install)
include(lagrange_limit_parallelism)
include(lagrange_warnings)
include(code-coverage)
include(sanitizers)

set(CMAKE_MACOSX_RPATH 1) # Set rpath for mac

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(LAGRANGE_LIMIT_PARALLELISM)
    lagrange_limit_parallelism()
endif()

find_package(Threads REQUIRED)

# Include OneTBB early to provide TBB::tbb target
if(LAGRANGE_WITH_ONETBB)
    set(MKL_VERSION 2021.3.0 CACHE STRING "MKL version to use (2020.4 or 2021.3.0)")
    include(onetbb)
else()
    set(MKL_VERSION 2020.4 CACHE STRING "MKL version to use (2020.4 or 2021.3.0)")
endif()

# Enable unit testing at the root level
if(LAGRANGE_UNIT_TESTS)
    include(CTest)

    # Include Catch2 and provide function `catch_discover_tests` to register tests.
    include(catch2)
    FetchContent_GetProperties(catch2)
    include("${catch2_SOURCE_DIR}/extras/Catch.cmake")

    # Global target for code coverage
    if(LAGRANGE_TOPLEVEL_PROJECT)
        add_code_coverage_all_targets()
    endif()
endif()

# Build modules
add_subdirectory(modules)

# Build documentation
if(LAGRANGE_DOCUMENTATION)
    add_subdirectory(docs)
endif()

# Build code extracted from markdown documentation
if(LAGRANGE_WEBSITE_REPO)
    add_subdirectory(docs/code)
endif()

# Copy shared dependencies for executables created by Lagrange. Clients using Lagrange as a subfolder must use their
# own mechanism to copy shared dlls into their executable folder. One possibility is to register their executable using
# `lagrange_add_executable`, and install dependencies by calling `lagrange_copy_all_runtime_dependencies` at the end of
# their own CMake script. Please follow https://gitlab.kitware.com/cmake/cmake/-/issues/20417 for an official solution
# when available.
if(LAGRANGE_COPY_RUNTIME_DEPS)
    lagrange_copy_all_runtime_dependencies()
endif()

################################################################################
# Install CMake config files
################################################################################

if(LAGRANGE_INSTALL)
    include(GNUInstallDirs)
    set(project_config_in "${PROJECT_SOURCE_DIR}/cmake/lagrange/lagrangeConfig.cmake.in")
    set(project_config_out "${CMAKE_CURRENT_BINARY_DIR}/LagrangeConfig.cmake")
    set(config_targets_file "LagrangeConfigTargets.cmake")
    set(version_config_file "${CMAKE_CURRENT_BINARY_DIR}/LagrangeConfigVersion.cmake")
    set(export_dest_dir "${CMAKE_INSTALL_LIBDIR}/cmake/lagrange")

    install(EXPORT Lagrange_Targets
        DESTINATION ${export_dest_dir}
        NAMESPACE lagrange::
        FILE ${config_targets_file}
        COMPONENT Lagrange_Development
    )

    include(CMakePackageConfigHelpers)
    configure_file("${project_config_in}" "${project_config_out}" @ONLY)

    write_basic_package_version_file("${version_config_file}" COMPATIBILITY SameMajorVersion)
    install(FILES "${project_config_out}" "${version_config_file}" DESTINATION "${export_dest_dir}")
endif()
