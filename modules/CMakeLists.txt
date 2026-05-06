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

# Embind / WebAssembly (not a Lagrange library module; see bindings/js/)
option(LAGRANGE_BINDINGS_JS "Build JavaScript/WebAssembly bindings" OFF)

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

    # Capture whether the user explicitly disabled this module before option() materializes
    # the cache variable (the default-OFF cache value would otherwise be indistinguishable
    # from an explicit -DLAGRANGE_MODULE_X=OFF on the command line).
    set(_user_disabled_module FALSE)
    if(DEFINED CACHE{${MODULE_OPTION}} AND NOT ${${MODULE_OPTION}})
        set(_user_disabled_module TRUE)
    endif()

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

    if(${MODULE_OPTION} OR (LAGRANGE_ALL AND NOT _user_disabled_module))
        lagrange_include_modules(${name})
    endif()
endforeach()

if(TARGET lagrange_python)
    lagrange_generate_python_binding_module()
endif()
