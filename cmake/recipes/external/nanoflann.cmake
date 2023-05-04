#
# Copyright 2019 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET nanoflann::nanoflann)
    return()
endif()

message(STATUS "Third-party (external): creating target 'nanoflann::nanoflann'")

include(CPM)
CPMAddPackage(
    NAME nanoflann
    GITHUB_REPOSITORY jlblancoc/nanoflann
    GIT_TAG v1.3.2
    DOWNLOAD_ONLY ON
)

add_library(nanoflann INTERFACE)
add_library(nanoflann::nanoflann ALIAS nanoflann)

include(GNUInstallDirs)
target_include_directories(nanoflann SYSTEM INTERFACE
    "$<BUILD_INTERFACE:${nanoflann_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_sources(nanoflann PRIVATE
    "${nanoflann_SOURCE_DIR}/include/nanoflann.hpp"
)

set_target_properties(nanoflann PROPERTIES FOLDER "third_party")

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME nanoflann)
install(FILES ${nanoflann_SOURCE_DIR}/include/nanoflann.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS nanoflann EXPORT Nanoflann_Targets)
install(EXPORT Nanoflann_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nanoflann NAMESPACE nanoflann::)
