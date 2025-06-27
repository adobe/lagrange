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

if (TARGET ufbx::ufbx)
    return()
endif()

include(CPM)
CPMAddPackage(
    NAME ufbx
    GITHUB_REPOSITORY ufbx/ufbx
    GIT_TAG a63ff0a47485328880b3300e7bcdf01413343a45
)

include(GNUInstallDirs)
add_library(ufbx STATIC ${ufbx_SOURCE_DIR}/ufbx.c)
add_library(ufbx::ufbx ALIAS ufbx)
target_include_directories(ufbx PUBLIC
    $<BUILD_INTERFACE:${ufbx_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(ufbx PROPERTIES FOLDER third_party)
set_target_properties(ufbx PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ufbx)
install(DIRECTORY ${ufbx_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS ufbx EXPORT Ufbx_Targets)
install(EXPORT Ufbx_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ufbx NAMESPACE ufbx::)
