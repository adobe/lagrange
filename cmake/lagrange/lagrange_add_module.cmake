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
function(lagrange_add_module_component module_name)
    get_filename_component(component_dir "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
    get_filename_component(component "${component_dir}" NAME)
    get_filename_component(extras_dir "${component_dir}" DIRECTORY)
    get_filename_component(module_dir "${extras_dir}" DIRECTORY)
    get_filename_component(detected_module_name "${module_dir}" NAME)
    if(NOT detected_module_name STREQUAL module_name)
        message(FATAL_ERROR
            "Component '${component}' is under module '${detected_module_name}', not '${module_name}'"
        )
    endif()

    set(module_target "lagrange_${module_name}")
    if(NOT TARGET ${module_target})
        message(FATAL_ERROR "Cannot add component '${component}': lagrange::${module_name} does not exist")
    endif()
    if(TARGET lagrange::${module_name}::${component})
        return()
    endif()
    if(NOT component MATCHES "^[A-Za-z_]+$")
        message(FATAL_ERROR
            "Invalid component name '${component}' for lagrange::${module_name}; "
            "component names may contain letters and underscores only"
        )
    endif()

    file(GLOB_RECURSE component_sources CONFIGURE_DEPENDS "*.cpp" "*.h")
    if(NOT component_sources)
        message(FATAL_ERROR "Component '${component}' for lagrange::${module_name} has no sources")
    endif()

    get_target_property(module_type ${module_target} TYPE)
    if(module_type STREQUAL "STATIC_LIBRARY")
        set(component_type STATIC)
    elseif(module_type STREQUAL "SHARED_LIBRARY")
        set(component_type SHARED)
    else()
        message(FATAL_ERROR "Compiled components require a static or shared base module")
    endif()

    string(TOUPPER "${module_name}" uc_module_name)
    string(TOUPPER "${component}" uc_component)
    set(component_target "lagrange_${module_name}_${component}")
    add_library(${component_target} ${component_type} ${component_sources})
    add_library(lagrange::${module_name}::${component} ALIAS ${component_target})
    target_link_libraries(${component_target} PUBLIC ${module_target})
    target_include_directories(${component_target} PRIVATE "${module_dir}/src")
    target_compile_definitions(
        ${component_target}
        PUBLIC "LAGRANGE_${uc_module_name}_WITH_${uc_component}"
    )
    if(module_type STREQUAL "SHARED_LIBRARY")
        target_compile_definitions(${component_target} PRIVATE "lagrange_${module_name}_EXPORTS")
    endif()
    target_compile_features(${component_target} PUBLIC cxx_std_17)
    lagrange_add_warnings(${component_target})
    target_code_coverage(${component_target} PUBLIC AUTO ALL EXCLUDE "${FETCHCONTENT_BASE_DIR}/*")

    set_target_properties(
        ${component_target}
        PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Modules/Components"
    )
    get_target_property(warnings_as_errors ${module_target} COMPILE_WARNING_AS_ERROR)
    if(NOT warnings_as_errors STREQUAL "warnings_as_errors-NOTFOUND")
        set_target_properties(
            ${component_target}
            PROPERTIES COMPILE_WARNING_AS_ERROR "${warnings_as_errors}"
        )
    endif()

    get_target_property(install_components ${module_target} LAGRANGE_MODULE_INSTALL_COMPONENTS)
    if(install_components)
        lagrange_install(${component_target})
    endif()
    set_target_properties(${component_target} PROPERTIES EXPORT_NAME "${module_name}::${component}")
    message(STATUS "Lagrange: enabling component 'lagrange::${module_name}::${component}'")
endfunction()

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
        set_target_properties(lagrange_${module_name} PROPERTIES LAGRANGE_MODULE_INSTALL_COMPONENTS TRUE)
        lagrange_install(lagrange_${module_name})
    else()
        set_target_properties(lagrange_${module_name} PROPERTIES LAGRANGE_MODULE_INSTALL_COMPONENTS FALSE)
    endif()
endfunction()
