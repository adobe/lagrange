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
# 1. define module
lagrange_add_module(INTERFACE)

# 2. dependencies
lagrange_find_package(nanoflann REQUIRED)
include(libigl)
target_link_libraries(lagrange_bvh INTERFACE
    lagrange::core
    nanoflann::nanoflann
    igl::core
)

# 3. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()

if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()
