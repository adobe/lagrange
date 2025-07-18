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
if(TARGET Spectra::Spectra)
    return()
endif()

message(STATUS "Third-party (external): creating target 'Spectra::Spectra'")

include(CPM)
CPMAddPackage(
    NAME Spectra
    GITHUB_REPOSITORY yixuan/spectra
    GIT_TAG 3af56416547197e28e5a9049fd3a56d8bd49fb5d
)

add_library(Spectra::Spectra ALIAS Spectra)
