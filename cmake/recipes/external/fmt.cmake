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
if(TARGET fmt::fmt)
    return()
endif()

message(STATUS "Third-party (external): creating target 'fmt::fmt'")

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "fmt")
option(FMT_INSTALL "Generate the install target." ON)

include(CPM)
CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 10.2.1
    # Other versions to test with:
    # GIT_TAG 10.1.1
    # GIT_TAG 9.1.0
    # GIT_TAG 8.1.1
)

set_target_properties(fmt PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(fmt PROPERTIES FOLDER third_party)
