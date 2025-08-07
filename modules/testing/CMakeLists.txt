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

# 1. define module
lagrange_add_module(NO_INSTALL)
set_target_properties(lagrange_testing PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests")

# 2. dependencies
include(catch2)
lagrange_include_modules(io)
target_link_libraries(lagrange_testing PUBLIC
    Catch2::Catch2
    Threads::Threads
    lagrange::io
)

# 3. test-specific properties
lagrange_download_data()
add_dependencies(lagrange_testing lagrange_download_data)

target_compile_definitions(lagrange_testing
    PUBLIC
        CATCH_CONFIG_ENABLE_BENCHMARKING
    PRIVATE
        TEST_DATA_DIR="${LAGRANGE_DATA_FOLDER}"
        TEST_OUTPUT_DIR="${CMAKE_BINARY_DIR}/tmp"
)

if(LAGRANGE_ENABLE_GPU_TESTS)
    target_compile_definitions(lagrange_testing PUBLIC LAGRANGE_ENABLE_GPU_TESTS)
endif()

if(LAGRANGE_USE_PCH)
    add_library(lagrange_testing_pch INTERFACE)
    target_link_libraries(lagrange_testing_pch INTERFACE lagrange_core_pch)
    target_precompile_headers(lagrange_testing_pch INTERFACE
        <lagrange/testing/common.h>
        <lagrange/testing/check_mesh.h>
    )
endif()

# Set flag to hide unit tests depending on private data availability
if(LAGRANGE_EXTERNAL_ONLY)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_CORP_FLAG=\"[.corp]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC -DLA_CORP_FLAG=\"[corp]\")
endif()

# CMake option to enable slow unit tests in ctest
option(LAGRANGE_SLOW_TESTS "Enable slow unit tests" OFF)
if(LAGRANGE_SLOW_TESTS)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_SLOW_FLAG=\"[slow]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC -DLA_SLOW_FLAG=\"[.slow]\")
endif()

# When slow tests are disabled, we still hide some tests in Debug only
if(LAGRANGE_SLOW_TESTS)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_SLOW_DEBUG_FLAG=\"[slow-debug]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC
        LA_SLOW_DEBUG_FLAG=$<IF:$<CONFIG:Debug>,\"[.slow-debug]\",\"[slow-debug]\">
    )
endif()

# Workaround for MB bug
target_compile_definitions(lagrange_testing PUBLIC -DLA_SKIP_MAYFAIL_FLAG=\"\")

# Generate main target
add_library(lagrange_testing_main ${CMAKE_CURRENT_SOURCE_DIR}/main/main.cpp)
target_link_libraries(lagrange_testing_main PUBLIC lagrange::core lagrange::testing)
add_library(lagrange::testing::main ALIAS lagrange_testing_main)
set_target_properties(lagrange_testing_main PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests")

set_target_properties(lagrange_testing_main PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Get stack trace information
if(EMSCRIPTEN)
    option(LAGRANGE_WITH_CPPTRACE "Enable cpptrace" OFF)
else()
    option(LAGRANGE_WITH_CPPTRACE "Enable cpptrace" ON)
endif()
if(LAGRANGE_WITH_CPPTRACE)
    include(cpptrace)
    target_compile_definitions(lagrange_testing_main PRIVATE -DLAGRANGE_WITH_CPPTRACE)
    target_link_libraries(lagrange_testing_main PRIVATE cpptrace::cpptrace)
endif()
