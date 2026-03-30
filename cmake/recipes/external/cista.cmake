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
if(TARGET cista::cista)
    return()
endif()

message(STATUS "Third-party (external): creating target 'cista::cista'")

include(CPM)
CPMAddPackage(
    NAME cista
    GITHUB_REPOSITORY felixguendling/cista
    GIT_TAG 4356022b0020dd924ab8afb3bf0199c07e9a9943 # ahead of v0.16 because of 6f9a254
    DOWNLOAD_ONLY ON
    OPTIONS
        "CISTA_FMT OFF"
)

add_library(cista INTERFACE)
add_library(cista::cista ALIAS cista)

target_include_directories(cista SYSTEM INTERFACE "${cista_SOURCE_DIR}/include")

set_target_properties(cista PROPERTIES FOLDER "third_party")

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME cista)
install(TARGETS cista EXPORT Cista_Targets INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT Cista_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cista NAMESPACE cista::)
