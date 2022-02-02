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
lagrange_add_module()
target_compile_features(lagrange_core PUBLIC cxx_std_17)

set_target_properties(lagrange_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(LAGRANGE_DISABLE_FPE "Disable floating point exception code" OFF)
if(LAGRANGE_DISABLE_FPE)
    target_compile_definitions(lagrange_core PRIVATE -DLA_DISABLE_FPE)
else()
    # Try to compile fpe.cpp, and disable if not supported (e.g. emscripten, apple m1, etc.)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lagrange_fpe_main.cpp.in" [[
        int main(void) {
            return 0;
        }
    ]])
    configure_file(
        ${CMAKE_CURRENT_BINARY_DIR}/lagrange_fpe_main.cpp.in
        ${CMAKE_CURRENT_BINARY_DIR}/lagrange_fpe_main.cpp
        COPYONLY
    )
    try_compile(
        LAGRANGE_SUPPORT_FPE
        ${CMAKE_CURRENT_BINARY_DIR}/lagrange_fpe
        SOURCES
            ${CMAKE_CURRENT_LIST_DIR}/src/fpe.cpp
            ${CMAKE_CURRENT_BINARY_DIR}/lagrange_fpe_main.cpp
    )
    if(NOT LAGRANGE_SUPPORT_FPE)
        message(WARNING "Lagrange: disabling unsupported FPE code")
        target_compile_definitions(lagrange_core PRIVATE -DLA_DISABLE_FPE)
    endif()
endif()

option(LAGRANGE_ASSERT_DEBUG_BREAK "Assert will break into debugger on failure" OFF)
if(LAGRANGE_ASSERT_DEBUG_BREAK)
    target_compile_definitions(lagrange_core PRIVATE -DLA_ENABLE_ASSERT_DEBUG_BREAK)
endif()

# 2. dependencies
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
    add_library(lagrange_core_pch INTERFACE)
    target_precompile_headers(lagrange_core_pch INTERFACE
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

        # Lagrange core
        <lagrange/common.h>
        <lagrange/Mesh.h>
        <lagrange/Logger.h>
        <lagrange/utils/assert.h>
        <lagrange/utils/range.h>
        <lagrange/utils/safe_cast.h>
    )
endif()

# 3. installation
if(LAGRANGE_INSTALL)
    set_target_properties(lagrange_core PROPERTIES EXPORT_NAME core)
    lagrange_install(lagrange_core)
endif()

# 4. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_PERFORMANCE_TESTS)
    add_subdirectory(performance)
endif()

if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()
