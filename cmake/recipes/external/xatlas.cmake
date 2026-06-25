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
if(TARGET xatlas::xatlas)
    return()
endif()

message(STATUS "Third-party (external): creating target 'xatlas::xatlas'")

include(CPM)
CPMAddPackage(
    NAME xatlas
    GITHUB_REPOSITORY jdumas/xatlas
    GIT_TAG 649fae81dd20801db55338bc86e0a7eab745ddf4
    DOWNLOAD_ONLY ON
)

add_library(xatlas STATIC
    ${xatlas_SOURCE_DIR}/source/xatlas/xatlas.cpp
    ${xatlas_SOURCE_DIR}/source/xatlas/xatlas.h
)

set_target_properties(xatlas PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "third_party"
)

target_include_directories(xatlas PUBLIC
    ${xatlas_SOURCE_DIR}/source
)

add_library(xatlas::xatlas ALIAS xatlas)

# Install rules. Use per-install COMPONENT to avoid leaking
# CMAKE_INSTALL_DEFAULT_COMPONENT_NAME to other recipes processed after this one.
install(DIRECTORY ${xatlas_BINARY_DIR}/source/xatlas
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT xatlas
)
install(TARGETS xatlas EXPORT xatlas_Targets COMPONENT xatlas)
install(EXPORT xatlas_Targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xatlas
    NAMESPACE xatlas::
    COMPONENT xatlas
)
