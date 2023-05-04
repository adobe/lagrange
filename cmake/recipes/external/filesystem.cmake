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
if(TARGET ghcFilesystem::ghc_filesystem)
    return()
endif()

message(STATUS "Third-party (external): creating target 'ghcFilesystem::ghc_filesystem'")

include(CPM)
CPMAddPackage(
    NAME filesystem
    GITHUB_REPOSITORY gulrak/filesystem
    GIT_TAG v1.5.4
)

target_compile_definitions(ghc_filesystem INTERFACE GHC_WIN_WSTRING_STRING_TYPE)
