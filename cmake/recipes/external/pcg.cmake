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
if(TARGET pcg::pcg)
    return()
endif()

message(STATUS "Third-party (external): creating target 'pcg::pcg'")

include(CPM)
CPMAddPackage(
    NAME pcg
    GITHUB_REPOSITORY imneme/pcg-cpp
    GIT_TAG 428802d1a5634f96bcd0705fab379ff0113bcf13
)

add_library(pcg::pcg INTERFACE IMPORTED GLOBAL)
target_include_directories(pcg::pcg INTERFACE "${pcg_SOURCE_DIR}/include")
