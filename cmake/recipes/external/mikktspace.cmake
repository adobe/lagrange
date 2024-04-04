#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET mikktspace::mikktspace)
    return()
endif()

# Original mikktspace has UBSan issues. See this PR for details:
# https://github.com/mmikk/MikkTSpace/pull/3
include(CPM)
CPMAddPackage(
    NAME mikktspace
    GITHUB_REPOSITORY mchudleigh/MikkTSpace
    GIT_TAG b57786fd7aa5404904985829ef8dda445d7feac0
)

add_library(mikktspace STATIC ${mikktspace_SOURCE_DIR}/mikktspace.c)
add_library(mikktspace::mikktspace ALIAS mikktspace)
target_include_directories(mikktspace PUBLIC ${mikktspace_SOURCE_DIR})

set_target_properties(mikktspace PROPERTIES FOLDER third_party)
