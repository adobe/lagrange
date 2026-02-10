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
include_guard(GLOBAL)

# Using full path because this is included early in our root CMakeLists.txt
include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)
CPMAddPackage(
    NAME CMakeExtraUtils
    GITHUB_REPOSITORY LecrisUT/CMakeExtraUtils
    GIT_TAG v0.4.1

    # There is a bug in CPM when using DOWNLOAD_ONLY that before the first project() call.
    # Because CPM attempts to bypass FetchContent, subsequent calls to FetchContent_GetProperties()
    # may not give any result. This seems to only happen when the package is added with DOWNLOAD_ONLY.
    #
    # https://github.com/cpm-cmake/CPM.cmake/issues/227
    #
    SOURCE_SUBDIR cmake
)

# Due to the bug mentioned above, ${cmakeextrautils_SOURCE_DIR} can point to either the subdir or
# the root dir of the CMakeExtraUtils, depending on whether the cache is already populated. This is
# really ugly, but have to work around it for now.
FetchContent_GetProperties(CMakeExtraUtils)
if(EXISTS ${cmakeextrautils_SOURCE_DIR}/DynamicVersion.cmake)
    include(${cmakeextrautils_SOURCE_DIR}/DynamicVersion.cmake)
else()
    include(${cmakeextrautils_SOURCE_DIR}/cmake/DynamicVersion.cmake)
endif()
