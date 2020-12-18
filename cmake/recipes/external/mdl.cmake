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
if(TARGET mdl::mdl)
    return()
endif()

message(STATUS "Third-party (external): creating target 'mdl::mdl'")

# TODO: Build from source using FetchContent instead?
include(FetchContent)
set(MDL_VERSION "2019.1.1")
set(MDL_URL "https://developer.nvidia.com/designworks/dl/mdl-sdk-${MDL_VERSION}")
set(MDL_FOLDER "${FETCHCONTENT_BASE_DIR}/mdl/${MDL_VERSION}")
set(MDL_ARCHIVE "${MDL_FOLDER}/archive.zip")

if(NOT EXISTS ${MDL_FOLDER})
    unset(MDL_DYNAMIC_LIBS CACHE)

    file(MAKE_DIRECTORY ${MDL_FOLDER})

    if(NOT EXISTS ${MDL_ARCHIVE})
        message(STATUS "Downloading MDL to " ${MDL_FOLDER})
        file(
            DOWNLOAD ${MDL_URL} ${MDL_ARCHIVE}
            TIMEOUT 6000
            SHOW_PROGRESS
            EXPECTED_HASH SHA1=ae98c980fa752fca481527b62f128b2115497862
            TLS_VERIFY ON
        )
    endif()

    set(MDL_EXTRACTED "${MDL_FOLDER}/mdl-sdk-317500.2554")

    if(NOT EXISTS ${MDL_EXTRACTED})
        message(STATUS "Extracting MDL ... ")
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf ${MDL_ARCHIVE}
            WORKING_DIRECTORY ${MDL_FOLDER})
    endif()

    file(GLOB MDL_FILE "${MDL_EXTRACTED}/*")
    file(COPY ${MDL_FILE} DESTINATION ${MDL_FOLDER})

    file(REMOVE_RECURSE ${MDL_EXTRACTED})
    file(REMOVE ${MDL_ARCHIVE})
endif()

if(WIN32)
    file(GLOB __MDL_DYNAMIC_LIBS "${MDL_FOLDER}/nt-x86-64/lib/*.dll")
elseif(APPLE)
    file(GLOB __MDL_DYNAMIC_LIBS "${MDL_FOLDER}/macosx-x86-64/lib/*.so")
elseif(UNIX)
    file(GLOB __MDL_DYNAMIC_LIBS "${MDL_FOLDER}/linux-x86-64/lib/*.so")
endif()

set(MDL_DYNAMIC_LIBS ${__MDL_DYNAMIC_LIBS} CACHE STRING "MDL_DYNAMIC_LIBS")

add_library(mdl::mdl INTERFACE IMPORTED GLOBAL)
message(STATUS "MDL Path: ${MDL_FOLDER}")
target_include_directories(mdl::mdl SYSTEM INTERFACE ${MDL_FOLDER}/include)
