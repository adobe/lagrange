#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET misha::misha)
    return()
endif()

message(STATUS "Third-party (external): creating target 'misha::misha'")

include(CPM)
CPMAddPackage(
    NAME Misha
    GITHUB_REPOSITORY mkazhdan/Misha
    GIT_TAG 53f43d2a8290ddebcc438a97da9d4f994d35fc6a
)

add_library(misha::misha INTERFACE IMPORTED GLOBAL)

include(GNUInstallDirs)
target_include_directories(misha::misha INTERFACE
    $<BUILD_INTERFACE:${Misha_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(misha::misha INTERFACE cxx_std_17)

set_target_properties(misha::misha PROPERTIES FOLDER third_party)
