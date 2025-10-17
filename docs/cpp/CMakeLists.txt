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
cmake_minimum_required(VERSION 3.16)
project(LagrangeDocs)

find_package(Doxygen REQUIRED)

# Doxygen Awesome for fancy CSS theme
include(ExternalProject)
include(FetchContent)
ExternalProject_Add(
    doxygen-awesome
    PREFIX "${FETCHCONTENT_BASE_DIR}/doxygen-awesome"
    GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
    GIT_TAG v2.4.0
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
)
ExternalProject_Get_property(doxygen-awesome SOURCE_DIR)
set(DOXYGEN_AWESOME_DIR "${SOURCE_DIR}")

# Collect module names
file(GLOB module_paths "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/*")
unset(module_names)
foreach(path IN ITEMS ${module_paths})
    if(IS_DIRECTORY "${path}")
        get_filename_component(name ${path} NAME)
        list(APPEND module_names ${name})
    endif()
endforeach()

# List public include folders
# Note: We could also get that from our CMake targets, but this works well enough
unset(DOXYGEN_INCLUDE_PATHS)
foreach(name IN ITEMS ${module_names})
    list(APPEND DOXYGEN_INCLUDE_PATHS "modules/${name}/include")
    list(APPEND DOXYGEN_INCLUDE_PATHS "modules/${name}/src")
endforeach()
list(JOIN DOXYGEN_INCLUDE_PATHS " " DOXYGEN_INCLUDE_PATHS)

# Exclude those folders from doxygen documentation
set(DOXYGEN_EXCLUDE_FOLDERS
    modules/ui/ibls/
    modules/ui/shaders/
)
list(JOIN DOXYGEN_EXCLUDE_FOLDERS " " DOXYGEN_EXCLUDE_FOLDERS)

set(DOXYGEN_LAYOUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/DoxygenLayout.xml)
set(DOXYGEN_WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(DOXYGEN_INPUT_DIR modules)
set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Output base dir for doxygen documentation")
set(DOXYGEN_OUTPUT_HTML html CACHE STRING "Output subdir for doxygen html")
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
set(DOXYFILE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/header.html)

# Replace variables inside @@ with the current values
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}) # Doxygen won't create this for us

add_custom_target(doc
                  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
                  WORKING_DIRECTORY ${DOXYGEN_WORKING_DIR}
                  COMMENT "Generating docs")

add_dependencies(doc doxygen-awesome)
