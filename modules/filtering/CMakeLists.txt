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
lagrange_add_module()
if(LAGRANGE_TOPLEVEL_PROJECT)
    set_target_properties(lagrange_filtering PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

# 2. dependencies
if(NOT EMSCRIPTEN)
    if(LAGRANGE_NO_INTERNAL)
        if(APPLE)
            # Without Nasoq, we simply use Accelerate on all macOS platforms
            set(BLA_VENDOR Apple)
            find_package(BLAS REQUIRED)
            target_compile_definitions(lagrange_filtering PRIVATE LA_SOLVER_ACCELERATE)
        else()
            # Hold off on using MKL for now to avoid bloating the package size.
            # include(blas) # Accelerate on arm64 macOS, MKL on other platforms
            # target_compile_definitions(lagrange_filtering PRIVATE LA_SOLVER_MKL)
            add_library(BLAS::BLAS INTERFACE IMPORTED GLOBAL)
        endif()
    else()
        include(blas) # Accelerate on macOS, MKL on other platforms

        # TODO: Move the solver selection logic to a dedicated module to avoid repeating this in individual modules.
        if(APPLE)
            target_compile_definitions(lagrange_filtering PRIVATE LA_SOLVER_ACCELERATE)
        else()
            target_compile_definitions(lagrange_filtering PRIVATE LA_SOLVER_MKL)
        endif()
    endif()
    target_link_libraries(lagrange_filtering PRIVATE BLAS::BLAS)
endif()

if(USE_SANITIZER MATCHES "([Tt]hread)")
    target_compile_definitions(lagrange_filtering PRIVATE LA_SANITIZE_THREAD)
endif()

include(shape_gradient_domain)
target_link_libraries(lagrange_filtering
    PUBLIC
        lagrange::core
    PRIVATE
        shape_gradient_domain::shape_gradient_domain
)

# 3. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LAGRANGE_MODULE_PYTHON)
    add_subdirectory(python)
endif()
