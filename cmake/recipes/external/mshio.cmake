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
if(TARGET mshio::mshio)
    return()
endif()

message(STATUS "Third-party (external): creating target 'mshio::mshio'")

include(CPM)
CPMAddPackage(
    NAME mshio
    GITHUB_REPOSITORY qnzhou/MshIO
    GIT_TAG 2a897f96ee76497d90dd8560325c232278aaea9d
)

set_target_properties(mshio PROPERTIES FOLDER third_party)
set_target_properties(mshio PROPERTIES POSITION_INDEPENDENT_CODE ON)
