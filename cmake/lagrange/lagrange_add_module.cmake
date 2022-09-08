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
function(lagrange_add_module)
    # Retrieve module name
    get_filename_component(module_path "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
    get_filename_component(module_name "${module_path}" NAME)

    # Retrieve options
    set(options INTERFACE)
    set(oneValueArgs "")
    set(multiValueArgs "")
    cmake_parse_arguments(OPTIONS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Define module
    if(OPTIONS_INTERFACE)
        add_library(lagrange_${module_name} INTERFACE)
        set(module_scope INTERFACE)
    else()
        add_library(lagrange_${module_name})
        set(module_scope PUBLIC)
    endif()
    add_library(lagrange::${module_name} ALIAS lagrange_${module_name})
    message(STATUS "Lagrange: creating target 'lagrange::${module_name}'")

    include(GNUInstallDirs)
    target_include_directories(lagrange_${module_name} ${module_scope}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    # Target sources
    file(GLOB_RECURSE INC_FILES "include/*.h")
    file(GLOB_RECURSE SRC_FILES "src/*.cpp")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/" PREFIX "Header Files" FILES ${INC_FILES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/" PREFIX "Source Files" FILES ${SRC_FILES})
    target_sources(lagrange_${module_name} PRIVATE ${INC_FILES} ${SRC_FILES})

    # Target folder for IDE
    set_target_properties(lagrange_${module_name} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Modules")

    # Enable code coverage for non-interface targets
    if(NOT OPTIONS_INTERFACE)
        include(FetchContent)
        target_code_coverage(lagrange_${module_name} ${module_scope} AUTO ALL EXCLUDE "${FETCHCONTENT_BASE_DIR}/*")
    endif()

endfunction()
