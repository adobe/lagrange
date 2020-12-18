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
project(Lagrange VERSION 4.0)

# Detects whether internal libs are available
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/cmake/recipes/internal")
    set(LAGRANGE_NO_INTERNAL OFF)
else()
    set(LAGRANGE_NO_INTERNAL ON)
endif()

# Meta target: ALL includes all optional modules and UI.
option(LAGRANGE_ALL               "Build all lagrange modules" OFF)

# Tests and examples
option(LAGRANGE_UNIT_TESTS        "Build all unit tests"             ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_PERFORMANCE_TESTS "Build all performance tests"      ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_EXAMPLES          "Build all examples"               ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_USE_TBB_STATIC    "Build with static TBB"            ON) # TODO: Switch to dynamic one day
option(LAGRANGE_USE_PCH           "Enable precompiled headers"       OFF)
option(LAGRANGE_INSTALL           "Enable installation"              ${LAGRANGE_TOPLEVEL_PROJECT})
option(LAGRANGE_DOCUMENTATION     "Build Doxygen documentation"      OFF)

# Third-party dependency management
option(LAGRANGE_EXTERNAL_FIRST    "Third-party libs: prefer public mirrors over corporate ones" OFF)
option(LAGRANGE_EXTERNAL_ONLY     "Third-party libs: only download from public mirrors"         ${LAGRANGE_NO_INTERNAL})

# Artifactory key file
set(LAGRANGE_ARTIFACTORY_KEYFILE "" CACHE FILEPATH "Path to secret artifactory key.")

# Dependent options
set(TBB_PREFER_STATIC ${LAGRANGE_USE_TBB_STATIC} CACHE BOOL "Use the static version of TBB for the alias target" FORCE)

# Set default minimum C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(LAGRANGE_EXTERNAL_ONLY)
    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/recipes/external/")
elseif(LAGRANGE_EXTERNAL_FIRST)
    list(APPEND CMAKE_MODULE_PATH
        "${PROJECT_SOURCE_DIR}/cmake/recipes/external/"
        "${PROJECT_SOURCE_DIR}/cmake/recipes/internal/"
    )
else()
    list(APPEND CMAKE_MODULE_PATH
        "${PROJECT_SOURCE_DIR}/cmake/recipes/internal/"
        "${PROJECT_SOURCE_DIR}/cmake/recipes/external/"
    )
endif()
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/lagrange/")


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
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)
    include(Catch)
endif()

# Build modules
add_subdirectory(modules)

# Build documentation
if(LAGRANGE_DOCUMENTATION)
    add_subdirectory(docs)
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
