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
if(TARGET glm::glm)
    return()
endif()

message(STATUS "Third-party (external): creating target 'glm::glm'")

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG tags/0.9.9.8
    GIT_PROGRESS FALSE
    GIT_SHALLOW TRUE
)
message(WARNING "glm is being deprecated, use Eigen instead")

option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_STATIC_LIBS "Build static library" OFF)
option(GLM_TEST_ENABLE "Build unit tests" OFF)
FetchContent_MakeAvailable(glm)
