#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET simde::simde)
    return()
endif()

message(STATUS "Third-party (external): creating target 'simde::simde'")

include(CPM)
CPMAddPackage(
    NAME simde
    GITHUB_REPOSITORY simd-everywhere/simde
    GIT_TAG 1747b2482589fe894d49989159421da08c2a8bcd
)

# Enables native aliases. Not ideal but makes it easier to convert old code.
target_compile_definitions(simde INTERFACE SIMDE_ENABLE_NATIVE_ALIASES)

# On MSVC ARM64, all NEON vector types (float32x4_t, int64x2_t, ...) are typedefs of the same
# __n128 type, so simde__m128 and simde__m128i would be identical to the type system. This breaks
# code that overloads on __m128 vs __m128i (e.g. WindingNumber's VM_SSEFunc.h). Disabling native
# NEON forces SIMDe to use its own distinct union types instead.
if(MSVC AND CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
    target_compile_definitions(simde INTERFACE SIMDE_NO_NATIVE)
endif()
