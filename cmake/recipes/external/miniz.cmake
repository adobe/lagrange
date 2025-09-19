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
if(TARGET miniz::miniz)
    return()
endif()

message(STATUS "Third-party (external): creating target 'miniz::miniz'")

include(CPM)
CPMAddPackage(
    NAME miniz
    URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
    URL_MD5 0604f14151944ff984444b04c5c760e5
)

add_library(miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
add_library(miniz::miniz ALIAS miniz)

include(GNUInstallDirs)
target_include_directories(miniz PUBLIC
    $<BUILD_INTERFACE:${miniz_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(miniz PROPERTIES FOLDER third_party)
set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(miniz PUBLIC
    $<BUILD_INTERFACE:${miniz_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME miniz)
install(FILES ${miniz_SOURCE_DIR}/miniz.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS miniz EXPORT Miniz_Targets)
install(EXPORT Miniz_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/miniz NAMESPACE miniz::)
