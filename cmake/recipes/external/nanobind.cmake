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
if(COMMAND nanobind_add_module)
    return()
endif()

message(STATUS "Third-party (external): creating target 'nanobind::nanobind'")

include(CPM)
CPMAddPackage(
    NAME nanobind
    GITHUB_REPOSITORY wjakob/nanobind
    GIT_TAG f2438bb73a1673e4ad9d0c84d353a88cf54e55bf
    DOWNLOAD_ONLY ON
)

include(python)
set(NB_SHARED OFF CACHE INTERNAL "")

# Note that we do not use FetchContent_MakeAvailable here because nanobind's cmake changes
# CMAKE_CXX_FLAGS and attempts to refind python, which can leads to cmake error.
find_package(nanobind PATHS ${nanobind_SOURCE_DIR}/cmake NO_DEFAULT_PATH)

