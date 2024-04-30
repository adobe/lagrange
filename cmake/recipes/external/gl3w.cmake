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
if(TARGET gl3w::gl3w)
    return()
endif()

message(STATUS "Third-party (external): creating target 'gl3w::gl3w'")

block()
    set(BUILD_SHARED_LIBS OFF)
    include(CPM)
    CPMAddPackage(
        NAME gl3w
        GITHUB_REPOSITORY adobe/lagrange-gl3w
        GIT_TAG a9e41479e30266cecb72df413f4f6d71b0228a71
    )
endblock()

add_library(gl3w::gl3w ALIAS gl3w)

set_target_properties(gl3w PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(gl3w PROPERTIES FOLDER third_party)
