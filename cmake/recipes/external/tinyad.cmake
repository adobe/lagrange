#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

if(TARGET tinyad::tinyad)
    return()
endif()

message(STATUS "Third-party (external): creating target 'tinyad::tinyad'")

include(CPM)
CPMAddPackage(
    NAME tinyad
    GITHUB_REPOSITORY patr-schm/TinyAD
    GIT_TAG 29417031c185b6dc27b6d4b684550d844459b735
    DOWNLOAD_ONLY ON
)

add_library(tinyad INTERFACE)
add_library(tinyad::tinyad ALIAS tinyad)

target_include_directories(tinyad
    INTERFACE
    ${tinyad_SOURCE_DIR}/include
)