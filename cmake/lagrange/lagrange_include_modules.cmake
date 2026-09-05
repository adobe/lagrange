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
# Includes one module and any requested optional components.
# Usage: lagrange_include_module(<module> [COMPONENTS <component>...]).
function(lagrange_include_module name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(PARSE_ARGV 1 OPTIONS "${options}" "${oneValueArgs}" "${multiValueArgs}")
    if(OPTIONS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unexpected arguments for lagrange_include_module(${name}): ${OPTIONS_UNPARSED_ARGUMENTS}"
        )
    endif()

    if(LAGRANGE_ALLOWLIST AND NOT (${name} IN_LIST LAGRANGE_ALLOWLIST))
        message(FATAL_ERROR "Lagrange module '${name}' is not allowed in this project")
    endif()
    if(${name} IN_LIST LAGRANGE_BLOCKLIST)
        message(FATAL_ERROR "Lagrange module '${name}' is excluded from this project")
    endif()

    get_property(lagrange_module_path GLOBAL PROPERTY __lagrange_module_path)
    get_property(lagrange_source_dir GLOBAL PROPERTY __lagrange_source_dir)
    get_property(lagrange_binary_dir GLOBAL PROPERTY __lagrange_binary_dir)
    set(CMAKE_MODULE_PATH ${lagrange_module_path})
    if(NOT TARGET lagrange::${name})
        add_subdirectory(
            ${lagrange_source_dir}/modules/${name}
            ${lagrange_binary_dir}/modules/lagrange_${name}
        )
    endif()

    foreach(component IN LISTS OPTIONS_COMPONENTS)
        if(TARGET lagrange::${name}::${component})
            continue()
        endif()
        if(NOT component MATCHES "^[A-Za-z_]+$")
            message(FATAL_ERROR
                "Invalid component name '${component}' for lagrange::${name}; "
                "component names may contain letters and underscores only"
            )
        endif()
        set(component_source_dir "${lagrange_source_dir}/modules/${name}/extras/${component}")
        if(NOT EXISTS "${component_source_dir}/CMakeLists.txt")
            file(GLOB available_component_dirs LIST_DIRECTORIES TRUE
                "${lagrange_source_dir}/modules/${name}/extras/*"
            )
            set(available_components "")
            foreach(component_dir IN LISTS available_component_dirs)
                if(IS_DIRECTORY "${component_dir}")
                    get_filename_component(available_component "${component_dir}" NAME)
                    list(APPEND available_components "${available_component}")
                endif()
            endforeach()
            message(FATAL_ERROR
                "Unknown component '${component}' for lagrange::${name}. "
                "Available components: ${available_components}"
            )
        endif()
        add_subdirectory(
            "${component_source_dir}"
            "${lagrange_binary_dir}/modules/lagrange_${name}/extras/${component}"
        )
        if(NOT TARGET lagrange::${name}::${component})
            message(FATAL_ERROR
                "Component '${component}' did not create lagrange::${name}::${component}"
            )
        endif()
    endforeach()
endfunction()

# Includes multiple base modules; use lagrange_include_module() for optional components.
function(lagrange_include_modules)
    set(module_names ${ARGN})
    if("COMPONENTS" IN_LIST module_names)
        message(FATAL_ERROR
            "lagrange_include_modules() accepts module names only; use "
            "lagrange_include_module(<module> COMPONENTS ...) for components"
        )
    endif()
    foreach(name IN LISTS module_names)
        lagrange_include_module(${name})
    endforeach()
endfunction()
