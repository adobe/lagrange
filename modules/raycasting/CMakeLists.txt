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
lagrange_include_modules(bvh)
target_link_libraries(lagrange_raycasting PUBLIC
    lagrange::core
    lagrange::bvh
)

option(LAGRANGE_WITH_EMBREE_4 "Use Embree4 with Lagrange" ON)
if(LAGRANGE_WITH_EMBREE_4)
    lagrange_find_package(embree 4 CONFIG REQUIRED)
    target_compile_definitions(lagrange_raycasting PUBLIC LAGRANGE_WITH_EMBREE_4)
    target_link_libraries(lagrange_raycasting PUBLIC embree::embree4)
else()
    lagrange_find_package(embree 3 CONFIG REQUIRED)
    target_link_libraries(lagrange_raycasting PUBLIC embree::embree)
endif()

option(LAGRANGE_EMBREE_DEBUG "Perform error-checking when performing each single Embree call"  OFF)
if(LAGRANGE_EMBREE_DEBUG)
    target_compile_definitions(lagrange_raycasting PUBLIC LAGRANGE_EMBREE_DEBUG)
endif()

# 3. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_PERFORMANCE_TESTS)
    add_subdirectory(performance)
endif()

if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()
