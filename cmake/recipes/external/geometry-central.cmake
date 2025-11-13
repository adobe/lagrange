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
if (TARGET geometry-central::geometry-central)
    return()
endif()

# Include dependencies before geometry-central tries to find them.
lagrange_find_package(happly REQUIRED)
lagrange_find_package(nanoflann REQUIRED)
lagrange_find_package(nanort REQUIRED)
lagrange_find_package(Eigen3 REQUIRED GLOBAL)

message(STATUS "Third-party (external): creating target 'geometry-central::geometry-central'")

# TODO: Patch upstream geometry-central to use the correct nanoflann::nanoflann target name
if(NOT TARGET nanoflann)
    add_library(nanoflann ALIAS nanoflann::nanoflann)
endif()

include(CPM)
CPMAddPackage(
    NAME geometry-central
    GITHUB_REPOSITORY jdumas/geometry-central
    GIT_TAG fcba48bfe8fa419c5469937ce9d77feccc69ebed
)

set_target_properties(geometry-central PROPERTIES FOLDER third_party)
add_library(geometry-central::geometry-central ALIAS geometry-central)
set_target_properties(geometry-central PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME geometry-central)
install(DIRECTORY ${geometry-central_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS geometry-central EXPORT GeometryCentral_Targets)
install(EXPORT GeometryCentral_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/geometry-central
    NAMESPACE geometry-central::)
