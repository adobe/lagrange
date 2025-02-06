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
if(TARGET nanort::nanort)
    return()
endif()

message(STATUS "Third-party (external): creating target 'nanort::nanort'")

set(NANORT_VERSION cdf474ae9176b87a9334c858bb06c9e028ed0e0d)
set(NANORT_URL "https://raw.githubusercontent.com/lighttransport/nanort/${NANORT_VERSION}/nanort.h")

include(CPM)
CPMAddPackage(
    NAME nanort
    URL ${NANORT_URL}
    URL_HASH MD5=8f6c22c5b57246809ba29b02467fdc2c
    DOWNLOAD_NO_EXTRACT 1
    DOWNLOAD_ONLY ON
)

add_library(nanort INTERFACE)
add_library(nanort::nanort ALIAS nanort)
target_include_directories(nanort INTERFACE
    $<BUILD_INTERFACE:${nanort_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(nanort PROPERTIES FOLDER third_party)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME nanort)
install(DIRECTORY ${NANORT_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS nanort EXPORT NanoRT_Targets)
install(EXPORT NanoRT_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nanort NAMESPACE nanort::)
