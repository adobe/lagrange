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

# Collect subfolder names
file(GLOB module_paths "./*")
unset(module_names)
foreach(path IN ITEMS ${module_paths})
    if(IS_DIRECTORY "${path}")
        get_filename_component(name ${path} NAME)
        if(NOT (${name} STREQUAL "core" OR ${name} STREQUAL "ui"))
            list(APPEND module_names ${name})
        endif()
    endif()
endforeach()
# Include UI module first
list(PREPEND module_names "ui")

################################################################################

# Always build core library
add_subdirectory(core)

# For subproject builds, check if client has enabled any of the module option,
# and warn about the behavior. To force new behavior to users, we produce an
# error if the old approach is detected.
if(NOT LAGRANGE_TOPLEVEL_PROJECT)
    if(LAGRANGE_ALL)
        message(SEND_ERROR "LAGRANGE_ALL is enabled in a subproject build. This behavior is deprecated. "
                           "Please use the helper function lagrange_include_modules() instead.")
    endif()
endif()

# Iterate over each module folder and include it
foreach(name IN ITEMS ${module_names})
    # Deduct module flag option from lowercase module name
    string(TOUPPER ${name} MODULE_FLAG)
    set(MODULE_OPTION LAGRANGE_MODULE_${MODULE_FLAG})

    # Allows setting the option
    option(${MODULE_OPTION} "Build module lagrange::${name}" OFF)

    # Warn about old option name and subproject build behavior
    if(LAGRANGE_${MODULE_FLAG})
        message(SEND_ERROR "LAGRANGE_${MODULE_FLAG} is the old name to enable the module lagrange::${name}. "
                           "Use ${MODULE_OPTION} instead, and clear your CMakeCache.txt")
    endif()
    if((NOT LAGRANGE_TOPLEVEL_PROJECT) AND ${MODULE_OPTION})
        message(SEND_ERROR "${MODULE_OPTION} is enabled in a subproject build. This behavior is deprecated. "
                           "Please use the helper function lagrange_include_modules() instead.")
    endif()

    if(${MODULE_OPTION} OR LAGRANGE_ALL)
        lagrange_include_modules(${name})
    endif()
endforeach()

if(TARGET lagrange_python)
    lagrange_generate_python_binding_module()
endif()
