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

# 1. define module
lagrange_add_module(INTERFACE)
if(LAGRANGE_TOPLEVEL_PROJECT)
    set_target_properties(lagrange_solver PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

# 2. dependencies
if(NOT EMSCRIPTEN AND NOT SKBUILD)
    # Note: For now we avoid using MKL in our public Python bindings, to avoid bloating up the size of
    # the uploaded wheels. The long-term solution is to depend on the PyPI package for MKL at build-time.

    include(blas) # Accelerate on macOS, MKL on other platforms
    if(APPLE)
        target_compile_definitions(lagrange_solver INTERFACE LA_SOLVER_ACCELERATE)
    else()
        target_compile_definitions(lagrange_solver INTERFACE LA_SOLVER_MKL)
    endif()
    target_link_libraries(lagrange_solver INTERFACE BLAS::BLAS)
endif()

if(USE_SANITIZER MATCHES "([Tt]hread)")
    target_compile_definitions(lagrange_solver INTERFACE LA_SANITIZE_THREAD)
endif()

lagrange_find_package(Eigen3 REQUIRED GLOBAL)
lagrange_find_package(TBB CONFIG REQUIRED)
target_link_libraries(lagrange_solver INTERFACE Eigen3::Eigen TBB::tbb)

# 3. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_MODULE_PYTHON)
    add_subdirectory(python)
endif()
