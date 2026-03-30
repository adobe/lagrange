#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if (TARGET instant-meshes-core::instant-meshes-core)
    return()
endif()

message(STATUS "Third-party (external): creating target 'instant-meshes-core::instant-meshes-core'")

include(CPM)
CPMAddPackage(
    NAME instant-meshes-core
    GITHUB_REPOSITORY qnzhou/instant-meshes-core
    GIT_TAG 8c87f12bec4b98ce29febcf5dd63ebb90e957104
)

add_library(instant-meshes-core::instant-meshes-core ALIAS instant-meshes-core)
set_target_properties(instant-meshes-core PROPERTIES FOLDER third_party)
set_target_properties(instant-meshes-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
