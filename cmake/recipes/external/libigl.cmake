#
# Copyright 2019 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET igl::core)
    return()
endif()

message(STATUS "Third-party (external): creating target 'igl::core'")

lagrange_find_package(Eigen3 REQUIRED)
option(LIBIGL_INSTALL "Enable installation of libigl targets" ON)

include(CPM)
CPMAddPackage(
    NAME libigl
    GITHUB_REPOSITORY libigl/libigl
    GIT_TAG 01f2dc0a60a07f78219e906679645656c7c5ee40
)

set_target_properties(igl_core PROPERTIES FOLDER third_party/libigl)
