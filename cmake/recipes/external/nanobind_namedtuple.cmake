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

# Provide Python and nanobind before adding the package: the upstream CMakeLists.txt skips its own
# find_package(Python) when Python::Module exists, and skips nanobind discovery (which probes the
# interpreter via `python -m nanobind`) when nanobind_add_module is available.
include(python)
include(nanobind)

include(CPM)
CPMAddPackage(
    NAME nanobind_namedtuple
    GITHUB_REPOSITORY jdumas/nanobind_namedtuple
    GIT_TAG 5eea9b5497c031ef556a250b04d47b68e54395c1
)

set_target_properties(nanobind_namedtuple PROPERTIES FOLDER third_party)
