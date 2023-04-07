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
function(lagrange_include_modules)
    get_property(lagrange_module_path GLOBAL PROPERTY __lagrange_module_path)
    set(CMAKE_MODULE_PATH ${lagrange_module_path})
    foreach(name IN ITEMS ${ARGN})
        if(name MATCHES "(volume|raycasting)")
            continue()
        endif()

        if(LAGRANGE_ALLOWLIST AND NOT (${name} IN_LIST LAGRANGE_ALLOWLIST))
            message(FATAL_ERROR "Lagrange module '${name}' is not allowed in this project")
        endif()
        if(${name} IN_LIST LAGRANGE_BLOCKLIST)
            message(FATAL_ERROR "Lagrange module '${name}' is excluded from this project")
        endif()

        if(NOT TARGET lagrange::${name})
            get_property(lagrange_source_dir GLOBAL PROPERTY __lagrange_source_dir)
            get_property(lagrange_binary_dir GLOBAL PROPERTY __lagrange_binary_dir)
            add_subdirectory(${lagrange_source_dir}/modules/${name} ${lagrange_binary_dir}/modules/lagrange_${name})
        endif()

        # deformers or lantern modules can be skipped on certain platforms due to lack of support
        if(NOT name MATCHES "(deformers|lantern)" AND NOT TARGET lagrange::${name})
            message(FATAL_ERROR "Failed to create lagrange module: ${name}")
        endif()
    endforeach()
endfunction()
