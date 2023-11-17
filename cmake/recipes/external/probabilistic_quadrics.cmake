#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if (TARGET probabilistic_quadrics::probabilistic_quadrics)
    return()
endif()

message(STATUS "Third-party: creating target 'probabilistic_quadrics::probabilistic_quadrics'")

include(CPM)
CPMAddPackage(
    NAME probabilistic_quadrics
    GIT_REPOSITORY https://github.com/Philip-Trettner/probabilistic-quadrics.git
    GIT_TAG 4920ade95eeaa65f61a70fffea3c389a023977a7
)

add_library(probabilistic_quadrics INTERFACE)
add_library(probabilistic_quadrics::probabilistic_quadrics ALIAS probabilistic_quadrics)

target_include_directories(probabilistic_quadrics
    INTERFACE
    ${probabilistic_quadrics_SOURCE_DIR}
)

target_compile_features(probabilistic_quadrics INTERFACE cxx_std_17)
set_target_properties(probabilistic_quadrics PROPERTIES FOLDER third_party)
