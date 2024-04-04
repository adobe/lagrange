#
# Copyright 2020 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET tinyexr::tinyexr)
    return()
endif()

message(STATUS "Third-party (external): creating target 'tinyexr::tinyexr'")

include(miniz)

include(CPM)
CPMAddPackage(
    NAME tinyexr
    GITHUB_REPOSITORY syoyo/tinyexr
    GIT_TAG v1.0.7
    DOWNLOAD_ONLY ON
)

add_library(tinyexr STATIC
    ${tinyexr_SOURCE_DIR}/tinyexr.h
    ${tinyexr_SOURCE_DIR}/tinyexr.cc
)
add_library(tinyexr::tinyexr ALIAS tinyexr)
set_target_properties(tinyexr PROPERTIES FOLDER third_party)
set_target_properties(tinyexr PROPERTIES POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
target_include_directories(tinyexr PUBLIC
    $<BUILD_INTERFACE:${tinyexr_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(tinyexr PRIVATE miniz::miniz)

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME tinyexr)
install(TARGETS tinyexr EXPORT Tinyexr_Targets)
install(EXPORT Tinyexr_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinyexr NAMESPACE tinyexr::)
install(DIRECTORY ${tinyexr_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
