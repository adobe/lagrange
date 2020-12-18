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
if(TARGET stb::stb)
    return()
endif()

message(STATUS "Third-party (external): creating target 'stb::stb'")

include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG f67165c2bb2af3060ecae7d20d6f731173485ad0
)
FetchContent_MakeAvailable(stb)

add_library(stb::stb INTERFACE IMPORTED GLOBAL)
target_include_directories(stb::stb INTERFACE "${stb_SOURCE_DIR}")
