#
# Copyright 2021 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET nonstd::span-lite)
    return()
endif()

message(STATUS "Third-party (external): creating target 'nonstd::span-lite'")

include(FetchContent)
FetchContent_Declare(
    span-lite
    GIT_REPOSITORY https://github.com/martinmoene/span-lite.git
    GIT_TAG 1d79b2325f176979aea526fb76a3692e011049a5
)
FetchContent_MakeAvailable(span-lite)

set_target_properties(span-lite PROPERTIES FOLDER third_party)

# On Windows, enable natvis files to improve debugging experience
if(WIN32)
    target_sources(span-lite INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/span-lite.natvis>)
endif()
