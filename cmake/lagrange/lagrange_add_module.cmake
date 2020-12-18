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
function(lagrange_add_module MODULE_NAME MODULE_DESCRIPTION)
    # Deduct module flag option from lowercase module name
    string(TOUPPER ${MODULE_NAME} MODULE_FLAG)
    set(MODULE_FLAG LAGRANGE_${MODULE_FLAG})

    # Allows setting the option
    option(${MODULE_FLAG} ${MODULE_DESCRIPTION} OFF)

    if(${MODULE_FLAG} OR LAGRANGE_ALL)
        # Create the target
        if(NOT TARGET lagrange::${MODULE_NAME})
            add_subdirectory(${MODULE_NAME})
        endif()
        if(NOT TARGET lagrange::${MODULE_NAME})
            message(FATAL_ERROR "Failed to create alias target for lagrange::${MODULE_NAME}")
        endif()

        if(TARGET lagrange_${MODULE_NAME}_)
            set_target_properties(lagrange_${MODULE_NAME}_ PROPERTIES FOLDER "Lagrange")
        elseif(TARGET lagrange_${MODULE_NAME})
            set_target_properties(lagrange_${MODULE_NAME} PROPERTIES FOLDER "Lagrange")
        endif()
    endif()
endfunction()
