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
if(TARGET fcbk::fcbk)
    return()
endif()

message(STATUS "Third-party (external): creating target 'fcbk::fcbk'")

include(CPM)
CPMAddPackage(
    NAME fcbk
    GITHUB_REPOSITORY fcBK-maxflow/fcBK
    GIT_TAG 78d5f92e0c835089c9473b77f8d32a14e782c703
)

set_target_properties(fcbk PROPERTIES FOLDER third_party)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME fcbk)
install(DIRECTORY ${fcbk_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS fcbk EXPORT fcbk_Targets)
install(EXPORT fcbk_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fcbk NAMESPACE fcbk::)
