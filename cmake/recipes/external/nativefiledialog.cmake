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
if(TARGET nativefiledialog::nativefiledialog)
    return()
endif()

message(STATUS "Third-party (external): creating target 'nativefiledialog::nativefiledialog'")

include(FetchContent)
FetchContent_Declare(
    nativefiledialog
    GIT_REPOSITORY https://github.com/mlabbe/nativefiledialog.git
    GIT_TAG 67345b80ebb429ecc2aeda94c478b3bcc5f7888e
)
FetchContent_MakeAvailable(nativefiledialog)

add_library(nativefiledialog)
add_library(nativefiledialog::nativefiledialog ALIAS nativefiledialog)

target_sources(nativefiledialog PRIVATE
    "${nativefiledialog_SOURCE_DIR}/src/include/nfd.h"
    "${nativefiledialog_SOURCE_DIR}/src/nfd_common.h"
    "${nativefiledialog_SOURCE_DIR}/src/nfd_common.c"
)
if(WIN32)
    target_sources(nativefiledialog PRIVATE "${nativefiledialog_SOURCE_DIR}/src/nfd_win.cpp")
elseif(APPLE)
    target_sources(nativefiledialog PRIVATE "${nativefiledialog_SOURCE_DIR}/src/nfd_cocoa.m")
elseif(UNIX)
    target_sources(nativefiledialog PRIVATE "${nativefiledialog_SOURCE_DIR}/src/nfd_gtk.c")

    # Use the package PkgConfig to detect GTK+ headers/library files
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    target_link_libraries(nativefiledialog PRIVATE PkgConfig::GTK3)
endif()

target_include_directories(nativefiledialog PRIVATE "${nativefiledialog_SOURCE_DIR}/src/")
target_include_directories(nativefiledialog PUBLIC "${nativefiledialog_SOURCE_DIR}/src/include/")

set_target_properties(nativefiledialog PROPERTIES FOLDER third_party)

# Warning config
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(nativefiledialog PRIVATE
        "-Wno-format-truncation"
    )
endif()
