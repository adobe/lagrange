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

# Detects whether this is a top-level project
get_directory_property(HAS_PARENT PARENT_DIRECTORY)
if(HAS_PARENT)
    set(LAGRANGE_TOPLEVEL_PROJECT OFF)
else()
    set(LAGRANGE_TOPLEVEL_PROJECT ON)
endif()

# Check required CMake version
set(REQUIRED_CMAKE_VERSION "3.16.0")
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

# Include user-provided default options if available. We do that before the main
# `project()` so that we can define the C/C++ compilers from the option file.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/LagrangeOptions.cmake)
    message(STATUS "Using local options file: ${CMAKE_CURRENT_SOURCE_DIR}/LagrangeOptions.cmake")
    include(${CMAKE_CURRENT_SOURCE_DIR}/LagrangeOptions.cmake)
endif()

################################################################################
project(Lagrange VERSION 4.1)

# Detects whether internal libs are available
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/cmake/recipes/internal")
    set(LAGRANGE_NO_INTERNAL OFF)
else()
    set(LAGRANGE_NO_INTERNAL ON)
endif()

# Meta target: ALL includes all optional modules and UI.
option(LAGRANGE_ALL               "Build all lagrange modules" OFF)

# When building anorigami module, defaults to using Eigen with MKL-dynamic and TBB-dynamic
if(LAGRANGE_ANORIGAMI OR (LAGRANGE_ALL AND NOT LAGRANGE_NO_INTERNAL))
    set(MKL_LINKING "dynamic" CACHE STRING "Linking strategy to use with MKL (static, dynamic or sdl)")
    option(EIGEN_WITH_MKL      "Use Eigen with MKL"       ON)
    option(TBB_PREFER_STATIC   "Build with static TBB"    OFF)
    option(TBB_BUILD_TBBMALLOC "Build TBB malloc library" ON)
endif()

# When building Arpack from source, we need to enable Fortran support in the top-level directory
if(ARPACK_BUILD_FROM_SOURCE)
    enable_language(Fortran)
endif()

# Tests and examples
option(LAGRANGE_UNIT_TESTS        "Build all unit tests"             ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_PERFORMANCE_TESTS "Build all performance tests"      ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_EXAMPLES          "Build all examples"               ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_USE_PCH           "Enable precompiled headers"       OFF)
option(LAGRANGE_INSTALL           "Enable installation"              ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_DOCUMENTATION     "Build Doxygen documentation"      OFF)
option(LAGRANGE_COPY_RUNTIME_DEPS "Copy runtime dependencies into executable folders" ${LAGRANGE_TOPLEVEL_PROJECT})

# Third-party dependency management
option(LAGRANGE_EXTERNAL_FIRST    "Third-party libs: prefer public mirrors over corporate ones" OFF)
option(LAGRANGE_EXTERNAL_ONLY     "Third-party libs: only download from public mirrors"         ${LAGRANGE_NO_INTERNAL})

# Artifactory key file
set(LAGRANGE_ARTIFACTORY_KEYFILE "" CACHE FILEPATH "Path to secret artifactory key.")

# Set default minimum C++ standard
if(LAGRANGE_TOPLEVEL_PROJECT)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
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


include(sanitizers)
include(lagrange_add_example)
include(lagrange_add_module)
include(lagrange_add_performance)
include(lagrange_add_test)
include(lagrange_global_flags)
include(lagrange_include_modules)
include(lagrange_install)
include(lagrange_warnings)

set(CMAKE_MACOSX_RPATH 1) # Set rpath for mac

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(Threads REQUIRED)

# Enable unit testing at the root level
if(LAGRANGE_UNIT_TESTS)
    include(CTest)
    enable_testing()

    # Include Catch2 and provide function `catch_discover_tests` to register tests.
    include(catch2)
    FetchContent_GetProperties(catch2)
    include("${catch2_SOURCE_DIR}/contrib/Catch.cmake")
endif()

# Build modules
add_subdirectory(modules)

# Build documentation
if(LAGRANGE_DOCUMENTATION)
    add_subdirectory(docs)
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
