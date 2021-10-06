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

unset(MDL_PREFIX)
unset(MDL_SUFFIX)
if(WIN32)
    set(MDL_PREFIX "nt-x86-64")
    set(MDL_SUFFIX ".dll")
elseif(APPLE)
    set(MDL_PREFIX "macosx-x86-64")
    set(MDL_SUFFIX ".so")
elseif(UNIX)
    set(MDL_PREFIX "linux-x86-64")
    set(MDL_SUFFIX ".so")
endif()

# Creates one IMPORTED target for each requested library
function(mdl_add_imported_library name)
    # Create imported target for library
    add_library(mdl::${name} MODULE IMPORTED GLOBAL)
    set_target_properties(mdl::${name} PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES CXX)

    # Important to avoid absolute path referencing of the libraries in the executables
    set_target_properties(mdl::${name} PROPERTIES IMPORTED_NO_SONAME TRUE)

    # Find library file
    string(TOUPPER mdl_${name}_lib LIBVAR)
    find_file(${LIBVAR}
        NAMES ${name}${MDL_SUFFIX}
        HINTS "${MDL_FOLDER}/${MDL_PREFIX}/lib"
        NO_DEFAULT_PATH
        REQUIRED
    )
    message(STATUS "Creating target mdl::${name} for file: ${${LIBVAR}}")

    # Set imported location
    set_target_properties(mdl::${name} PROPERTIES
        IMPORTED_LOCATION "${${LIBVAR}}"
    )

    # Add as dependency to the meta target mdl::mdl
    target_link_libraries(mdl::mdl INTERFACE mdl::${name})
endfunction()

add_library(mdl::mdl INTERFACE IMPORTED GLOBAL)
message(STATUS "MDL include dir: ${MDL_FOLDER}/include")
target_include_directories(mdl::mdl SYSTEM INTERFACE ${MDL_FOLDER}/include)

foreach(name IN ITEMS dds libmdl_sdk nv_freeimage)
    mdl_add_imported_library(${name})
endforeach()
