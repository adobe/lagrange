#
# Copyright 2019 Adobe. All rights reserved.
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

# 2. dependencies
include(embree)
lagrange_include_modules(bvh)
target_link_libraries(lagrange_raycasting PUBLIC
    lagrange::core
    lagrange::bvh
    embree::embree
)

# 3. options
option(LAGRANGE_EMBREE_DEBUG "Perform error-checking when performing each single Embree call"  OFF)
if(LAGRANGE_EMBREE_DEBUG)
    target_compile_definitions(lagrange_raycasting PUBLIC LAGRANGE_EMBREE_DEBUG)
endif()

# 4. installation
if(LAGRANGE_INSTALL)
    set_target_properties(lagrange_raycasting PROPERTIES EXPORT_NAME raycasting)
    lagrange_install(lagrange_raycasting)
endif()

# 5. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_PERFORMANCE_TESTS)
    add_subdirectory(performance)
endif()

if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()
