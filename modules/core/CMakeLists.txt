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
add_library(lagrange_core)
add_library(lagrange::core ALIAS lagrange_core)
message(STATUS "Lagrange: creating target 'lagrange::core'")

include(GNUInstallDirs)
target_include_directories(lagrange_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(lagrange_core PUBLIC cxx_std_17)

set_target_properties(lagrange_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(LAGRANGE_KEEP_TRANSITION_CODE "Keep transition code kept for backward compatibility with the old edge API." ON)
if(LAGRANGE_KEEP_TRANSITION_CODE)
    target_compile_definitions(lagrange_core PUBLIC -DLA_KEEP_TRANSITION_CODE)
endif()

option(LAGRANGE_DISABLE_FPE "Disable floating point exception code" OFF)
if(LAGRANGE_DISABLE_FPE)
    target_compile_definitions(lagrange_core PRIVATE -DLA_DISABLE_FPE)
else()
    # Try to compile fpe.cpp, and disable if not supported (e.g. emscripten, apple m1, etc.)
    try_compile(LAGRANGE_SUPPORT_FPE ${CMAKE_CURRENT_BINARY_DIR}/lagrange_fpe ${CMAKE_CURRENT_LIST_DIR}/src/fpe.cpp)
    if(NOT LAGRANGE_SUPPORT_FPE)
        message(WARNING "Lagrange: disabling unsupported FPE code")
        target_compile_definitions(lagrange_core PRIVATE -DLA_DISABLE_FPE)
    endif()
endif()

# 2. target sources
file(GLOB_RECURSE INC_FILES "include/*.h")
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/" FILES ${INC_FILES})
target_sources(lagrange_core PRIVATE ${INC_FILES} ${SRC_FILES})

# 3. dependencies
include(eigen)
include(libigl)
include(spdlog)
include(tbb)
target_link_libraries(lagrange_core PUBLIC
    lagrange::warnings
    Eigen3::Eigen
    igl::core
    spdlog::spdlog
    TBB::tbb
)

if(LAGRANGE_WITH_TRACY)
    include(tracy)
    target_link_libraries(lagrange_core PUBLIC tracy::client)
endif()

# precompile headers target. Link to this target in tests or modules with
# lots of compilation units (.cpp files) to speed up compilation time!
if(LAGRANGE_USE_PCH)
    add_library(lagrange_shared_pch INTERFACE)
    add_library(lagrange::shared_pch ALIAS lagrange_shared_pch)
    target_precompile_headers(lagrange_shared_pch INTERFACE
        # This list should contain files that are used often, ideally by
        # at least a third of the compilation units (directly or indirectly).

        # C headers
        <cmath>
        <cstdlib>

        # C++ headers
        <algorithm>
        <chrono>
        <functional>
        <limits>
        <memory>
        <numeric>
        <sstream>
        <string>
        <type_traits>
        <utility>

        # C++ datastructures
        <array>
        <list>
        <map>
        <queue>
        <unordered_map>
        <unordered_set>
        <vector>

        # Third party libraries
        <Eigen/Core>
    )

    add_library(lagrange_core_pch INTERFACE)
    add_library(lagrange::core_pch ALIAS lagrange_core_pch)
    target_link_libraries(lagrange_core_pch INTERFACE lagrange_shared_pch)
    target_precompile_headers(lagrange_core_pch INTERFACE
        <lagrange/Mesh.h>
        <lagrange/Logger.h>
        <lagrange/utils/la_assert.h>
        <lagrange/utils/range.h>
        <lagrange/utils/safe_cast.h>
    )
endif()

# 4. installation
if(LAGRANGE_INSTALL)
    set_target_properties(lagrange_core PROPERTIES EXPORT_NAME core)
    lagrange_install(lagrange_core)
endif()

# 5. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_PERFORMANCE_TESTS)
    add_subdirectory(performance)
endif()

if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()
