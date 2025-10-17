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
    set(options INTERFACE NO_INSTALL)
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

    set_target_properties(lagrange_${module_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    include(GNUInstallDirs)
    target_include_directories(lagrange_${module_name} ${module_scope}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_compile_features(lagrange_${module_name} ${module_scope} cxx_std_17)

    lagrange_add_warnings(lagrange_${module_name})

    # Target sources
    file(GLOB_RECURSE INC_FILES "include/*.h")
    file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/" PREFIX "Header Files" FILES ${INC_FILES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/" PREFIX "Source Files" FILES ${SRC_FILES})
    message(STATUS "Module ${module_name} using scope: ${module_scope}")
    target_sources(lagrange_${module_name}
        PRIVATE
            ${SRC_FILES}
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS
                include
            FILES
                ${INC_FILES}
    )

    # Export headers for shared libraries
    if(NOT OPTIONS_INTERFACE)
        string(TOUPPER ${module_name} uc_module_name)
        get_target_property(module_type lagrange_${module_name} TYPE)
        if(module_type STREQUAL "STATIC_LIBRARY")
            target_compile_definitions(lagrange_${module_name} PUBLIC "LA_${uc_module_name}_STATIC_DEFINE")
        endif()
    endif()

    # Target folder for IDE
    set_target_properties(lagrange_${module_name} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Modules")

    # Enable code coverage for non-interface targets
    if(NOT OPTIONS_INTERFACE)
        include(FetchContent)
        target_code_coverage(lagrange_${module_name} ${module_scope} AUTO ALL EXCLUDE "${FETCHCONTENT_BASE_DIR}/*")
    endif()

    # Create install rules
    if(LAGRANGE_INSTALL AND NOT OPTIONS_NO_INSTALL)
        lagrange_install(lagrange_${module_name})
    endif()

endfunction()
