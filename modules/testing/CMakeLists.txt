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
add_library(lagrange_testing)
add_library(lagrange::testing ALIAS lagrange_testing)
message(STATUS "Lagrange: creating target 'lagrange::testing'")

include(GNUInstallDirs)
target_include_directories(lagrange_testing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 2. target sources
file(GLOB_RECURSE INC_FILES "include/*.h")
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
target_sources(lagrange_testing PRIVATE ${INC_FILES} ${SRC_FILES})
set_target_properties(lagrange_testing PROPERTIES FOLDER "Lagrange//Tests")

# 3. dependencies
lagrange_include_modules(io)
target_link_libraries(lagrange_testing PUBLIC
    Catch2::Catch2
    Threads::Threads
    lagrange::io
)

# 4. test-specific properties
include(lagrange_download_data)
add_dependencies(lagrange_testing lagrange_download_data)

target_compile_definitions(lagrange_testing
    PUBLIC
        CATCH_CONFIG_ENABLE_BENCHMARKING
    PRIVATE
        TEST_DATA_DIR="${LAGRANGE_DATA_FOLDER}"
)

if(LAGRANGE_USE_PCH)
    target_link_libraries(lagrange_testing PUBLIC lagrange::core_pch)
endif()

# Set flag to hide unit tests depending on private data availability
if(LAGRANGE_EXTERNAL_ONLY)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_CORP_FLAG=\"[.corp]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC -DLA_CORP_FLAG=\"[corp]\")
endif()

if(LAGRANGE_CI_BUILD)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_MAYFAIL_FLAG=\"[.]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC -DLA_MAYFAIL_FLAG=\"[!mayfail]\")
endif()
