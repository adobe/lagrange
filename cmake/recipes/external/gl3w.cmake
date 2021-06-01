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
if(TARGET gl3w::gl3w)
    return()
endif()

message(STATUS "Third-party (external): creating target 'gl3w::gl3w'")

include(FetchContent)
FetchContent_Declare(
    gl3w
    GIT_REPOSITORY https://github.com/adobe/lagrange-gl3w.git
    GIT_TAG a9e41479e30266cecb72df413f4f6d71b0228a71
)
FetchContent_MakeAvailable(gl3w)
add_library(gl3w::gl3w ALIAS gl3w)

set_target_properties(gl3w PROPERTIES FOLDER third_party)
