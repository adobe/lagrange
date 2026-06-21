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
