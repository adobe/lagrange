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
include(happly)
include(nanoflann)
include(nanort)

message(STATUS "Third-party (external): creating target 'geometry-central::geometry-central'")

include(CPM)
CPMAddPackage(
    NAME geometry-central
    GITHUB_REPOSITORY qnzhou/geometry-central
    GIT_TAG 532359bcef04703360aa9300c5cd904faa372639
)

set_target_properties(geometry-central PROPERTIES FOLDER third_party)
add_library(geometry-central::geometry-central ALIAS geometry-central)

set_target_properties(geometry-central PROPERTIES FOLDER third_party)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME geometry-central)
install(DIRECTORY ${geometry-central_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS geometry-central EXPORT GeometryCentral_Targets)
install(EXPORT GeometryCentral_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/geometry-central
    NAMESPACE geometry-central::)
