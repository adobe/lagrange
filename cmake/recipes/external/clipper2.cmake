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
if(TARGET Clipper2::Clipper2)
    return()
endif()

message(STATUS "Third-party (external): creating target 'Clipper2::Clipper2'")

include(FetchContent)
FetchContent_Declare(
    clipper2
    GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
    GIT_TAG Clipper2_1.0.6
    SOURCE_SUBDIR CPP
)

option(CLIPPER2_UTILS "Build utilities" OFF)
option(CLIPPER2_EXAMPLES "Build examples" OFF)
option(CLIPPER2_TESTS "Build tests" OFF)
FetchContent_MakeAvailable(clipper2)

add_library(Clipper2::Clipper2 ALIAS Clipper2)

include(GNUInstallDirs)
target_include_directories(Clipper2 PUBLIC
    $<BUILD_INTERFACE:${Clipper2_SOURCE_DIR}/Clipper2Lib/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(Clipper2 PROPERTIES FOLDER third_party)
