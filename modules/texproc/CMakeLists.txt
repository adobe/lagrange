#
# Copyright 2025 Adobe. All rights reserved.
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
if(LAGRANGE_TOPLEVEL_PROJECT)
    set_target_properties(lagrange_texproc PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

# 2. dependencies
include(texture_signal_processing)
lagrange_include_modules(image solver)
target_link_libraries(lagrange_texproc
    PUBLIC
        lagrange::core
        lagrange::image
    PRIVATE
        lagrange::solver
        texture_signal_processing::texture_signal_processing
)

# 3. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LAGRANGE_MODULE_PYTHON)
   add_subdirectory(python)
endif()
