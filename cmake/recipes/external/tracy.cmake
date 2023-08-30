#
# Copyright 2021 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET tracy::client)
    return()
endif()

message(STATUS "Third-party (external): creating target 'tracy::client'")

include(CPM)
CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    GIT_TAG v0.7.7
)

################################################################################
# Client
################################################################################

# Do not forget to add global compilation flags '-fno-omit-frame-pointer' and '-g'!
add_library(tracy_client ${tracy_SOURCE_DIR}/TracyClient.cpp)
add_library(tracy::client ALIAS tracy_client)

include(GNUInstallDirs)
target_include_directories(tracy_client SYSTEM INTERFACE
    $<BUILD_INTERFACE:${tracy_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(tracy_client PUBLIC TRACY_ENABLE)

target_compile_features(tracy_client PUBLIC cxx_std_11)
set_target_properties(tracy_client PROPERTIES ENABLE_EXPORTS ON)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME tracy)
install(TARGETS tracy_client EXPORT Tracy_Targets)
install(EXPORT Tracy_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tracy NAMESPACE tracy::)
