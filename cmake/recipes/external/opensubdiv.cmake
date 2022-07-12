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
if(TARGET opensubdiv::opensubdiv)
    return()
endif()

message(STATUS "Third-party (external): creating target 'opensubdiv::opensubdiv'")

include(FetchContent)
FetchContent_Declare(
    opensubdiv
    GIT_REPOSITORY https://github.com/PixarAnimationStudios/OpenSubdiv.git
    GIT_TAG tags/v3_4_0
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(opensubdiv)
if(NOT opensubdiv_POPULATED)
    FetchContent_Populate(opensubdiv)
endif()

add_library(opensubdiv)
add_library(opensubdiv::opensubdiv ALIAS opensubdiv)

set_target_properties(opensubdiv PROPERTIES FOLDER third_party)

include(GNUInstallDirs)
target_include_directories(opensubdiv SYSTEM PUBLIC
    $<BUILD_INTERFACE:${opensubdiv_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(CMAKE_HOST_WIN32)
    target_compile_definitions(opensubdiv PUBLIC _USE_MATH_DEFINES)
endif()

file(GLOB SRC_FILES
    "${opensubdiv_SOURCE_DIR}/opensubdiv/far/*.h"
    "${opensubdiv_SOURCE_DIR}/opensubdiv/far/*.cpp"
    "${opensubdiv_SOURCE_DIR}/opensubdiv/sdc/*.h"
    "${opensubdiv_SOURCE_DIR}/opensubdiv/sdc/*.cpp"
    "${opensubdiv_SOURCE_DIR}/opensubdiv/vtr/*.h"
    "${opensubdiv_SOURCE_DIR}/opensubdiv/vtr/*.cpp"
)
source_group(
    TREE "${opensubdiv_SOURCE_DIR}/opensubdiv/"
    FILES ${SRC_FILES}
)
target_sources(opensubdiv PRIVATE ${SRC_FILES})

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(opensubdiv PRIVATE
        "-Wno-unused-function"
    )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(opensubdiv PRIVATE
        "-Wno-class-memaccess"
        "-Wno-cast-function-type"
        "-Wno-strict-aliasing"
    )
endif()

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME opensubdiv)
install(DIRECTORY ${opensubdiv_SOURCE_DIR}/opensubdiv DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS opensubdiv EXPORT Opensubdiv_Targets)
install(EXPORT Opensubdiv_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opensubdiv NAMESPACE opensubdiv::)
