#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

if(NOT LINUX)
    return()
endif()

find_program(patchelf_EXECUTABLE patchelf)

if(NOT patchelf_EXECUTABLE)
    # Hardcoded for x86_64 for now
    if(NOT CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL x86_64)
        message(FATAL_ERROR "Missing hash for architecture ${CMAKE_HOST_SYSTEM_PROCESSOR} for downloading patchelf binary.")
    endif()

    # Download patchelf binary
    include(CPM)
    CPMAddPackage(
        NAME patchelf
        URL https://github.com/NixOS/patchelf/releases/download/0.18.0/patchelf-0.18.0-x86_64.tar.gz
        URL_HASH MD5=70a12bd6ddde7eb3768e8ebb89506367
        DOWNLOAD_ONLY ON
    )
    find_program(patchelf_EXECUTABLE patchelf
        REQUIRED NO_DEFAULT_PATH
        HINTS ${patchelf_SOURCE_DIR}/bin
    )
endif()
