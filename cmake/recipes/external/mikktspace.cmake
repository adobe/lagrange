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

include(FetchContent)
FetchContent_Declare(
    mikktspace
    GIT_REPOSITORY https://github.com/mmikk/MikkTSpace.git
    GIT_TAG 3e895b49d05ea07e4c2133156cfa94369e19e409
)
FetchContent_MakeAvailable(mikktspace)

add_library(mikktspace ${mikktspace_SOURCE_DIR}/mikktspace.c)
add_library(mikktspace::mikktspace ALIAS mikktspace)
target_include_directories(mikktspace PUBLIC ${mikktspace_SOURCE_DIR})
