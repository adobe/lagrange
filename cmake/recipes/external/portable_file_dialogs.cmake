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
if(TARGET portable_file_dialogs::portable_file_dialogs)
    return()
endif()

message(STATUS "Third-party (external): creating target 'portable_file_dialogs::portable_file_dialogs'")

include(FetchContent)
FetchContent_Declare(
    portable_file_dialogs
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG 7a7a9f5fa800b28331672742c0e774d46186e34f
)
FetchContent_GetProperties(portable_file_dialogs)
if(portable_file_dialogs_POPULATED)
    return()
endif()
FetchContent_Populate(portable_file_dialogs)

# Define portable_file_dialogs library
add_library(portable_file_dialogs INTERFACE)
add_library(portable_file_dialogs::portable_file_dialogs ALIAS portable_file_dialogs)

include(GNUInstallDirs)
target_include_directories(portable_file_dialogs SYSTEM INTERFACE
    $<BUILD_INTERFACE:${portable_file_dialogs_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(portable_file_dialogs PROPERTIES FOLDER third_party)
