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
if(TARGET happly::happly)
    return()
endif()

message(STATUS "Third-party (external): creating target 'happly::happly'")

include(CPM)
CPMAddPackage(
    NAME happly
    # TODO: Revert back to upstream repo once https://github.com/nmwsharp/happly/pull/41 is merged.
    GITHUB_REPOSITORY jdumas/happly
    GIT_TAG 2486e7e13c5f0932e6d91064ad1fde46ee3f61f4
)

# Define happly library
add_library(happly INTERFACE ${happly_SOURCE_DIR}/happly.h)
add_library(happly::happly ALIAS happly)

include(GNUInstallDirs)
target_include_directories(happly INTERFACE
    $<BUILD_INTERFACE:${happly_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(happly PROPERTIES FOLDER third_party)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME happly)
install(DIRECTORY ${happly_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS happly EXPORT Happly_Targets)
install(EXPORT Happly_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/happly NAMESPACE happly::)
