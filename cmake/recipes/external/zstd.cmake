#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET zstd::libzstd)
    return()
endif()

message(STATUS "Third-party (external): creating target 'zstd::libzstd'")

include(CPM)
CPMAddPackage(
    NAME zstd
    GITHUB_REPOSITORY facebook/zstd
    GIT_TAG v1.5.6
    SOURCE_SUBDIR build/cmake
    OPTIONS
        "ZSTD_BUILD_SHARED OFF"
        "ZSTD_BUILD_PROGRAMS OFF"
        "ZSTD_BUILD_TESTS OFF"
        "ZSTD_BUILD_CONTRIB OFF"
        "ZSTD_MULTITHREAD_SUPPORT ON"
)

if(NOT TARGET zstd::libzstd)
    if(TARGET libzstd_static)
        add_library(zstd::libzstd ALIAS libzstd_static)
    elseif(TARGET libzstd_shared)
        add_library(zstd::libzstd ALIAS libzstd_shared)
    endif()
endif()

if(TARGET libzstd_static)
    set_target_properties(libzstd_static PROPERTIES FOLDER "third_party")
endif()
