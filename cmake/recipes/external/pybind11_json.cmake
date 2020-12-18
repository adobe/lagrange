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
if(TARGET pybind11_json::pybind11_json)
    return()
endif()

message(STATUS "Third-party (external): creating target 'pybind11_json::pybind11_json'")

include(FetchContent)
FetchContent_Declare(
    pybind11_json
    GIT_REPOSITORY https://github.com/pybind/pybind11_json.git
    GIT_TAG 0.2.6
    GIT_SHALLOW TRUE
)

include(pybind11)
include(nlohmann_json)
FetchContent_MakeAvailable(pybind11_json)

add_library(pybind11_json::pybind11_json ALIAS pybind11_json)
target_link_libraries(pybind11_json INTERFACE nlohmann_json::nlohmann_json)
