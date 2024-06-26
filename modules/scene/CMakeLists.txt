#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
# 1. define module
lagrange_add_module()
set_target_properties(lagrange_scene PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(LAGRANGE_TOPLEVEL_PROJECT)
    set_target_properties(lagrange_scene PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

lagrange_include_modules(fs)

# 2. dependencies
target_link_libraries(lagrange_scene
    lagrange::core
    lagrange::fs
)

# 3. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_MODULE_PYTHON)
    add_subdirectory(python)
endif()
