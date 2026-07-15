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
if(TARGET nanobind_namedtuple::nanobind_namedtuple)
    return()
endif()

message(STATUS "Third-party (external): creating target 'nanobind_namedtuple::nanobind_namedtuple'")

include(CPM)
CPMAddPackage(
    NAME nanobind_namedtuple
    GITHUB_REPOSITORY jdumas/nanobind_namedtuple
    GIT_TAG 38daf55e8100c903e511d422566362befd495104
    DOWNLOAD_ONLY ON
)

# The upstream CMakeLists.txt runs its own find_package(Python)/find_package(nanobind), which is
# redundant within Lagrange's build (nanobind is already fetched via CPM). Since the library is
# header-only, we declare the interface target manually instead.
add_library(nanobind_namedtuple INTERFACE)
add_library(nanobind_namedtuple::nanobind_namedtuple ALIAS nanobind_namedtuple)

target_include_directories(nanobind_namedtuple INTERFACE
    $<BUILD_INTERFACE:${nanobind_namedtuple_SOURCE_DIR}/include>
)

target_compile_features(nanobind_namedtuple INTERFACE cxx_std_17)

set_target_properties(nanobind_namedtuple PROPERTIES FOLDER third_party)
