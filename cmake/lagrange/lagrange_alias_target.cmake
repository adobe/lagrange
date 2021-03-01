#
# Copyright 2021 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

# Create an ALIAS for a given target. If the target is already an ALIAS, references the original target instead.
function(lagrange_alias_target name target)
    get_target_property(_aliased ${target} ALIASED_TARGET)
    if(_aliased)
        message(STATUS "Creating '${name}' as a new ALIAS target for '${_aliased}'.")
        add_library(${name} ALIAS ${_aliased})
    else()
        add_library(${name} ALIAS ${target})
    endif()
endfunction()
