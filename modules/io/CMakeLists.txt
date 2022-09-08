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
lagrange_add_module()

# 2. dependencies
lagrange_include_modules(core fs)
include(tinyobjloader)
include(libigl) # TODO: remove libigl later
include(mshio)
target_link_libraries(lagrange_io
    PUBLIC
        lagrange::core
        lagrange::fs
        tinyobjloader::tinyobjloader
        igl::core
    PRIVATE
        mshio::mshio
)

option(LAGRANGE_WITH_ASSIMP "Add assimp functionality to lagrange::io" FALSE)
if(LAGRANGE_WITH_ASSIMP)
    include(assimp)
    target_link_libraries(lagrange_io PUBLIC assimp::assimp)
    target_compile_definitions(lagrange_io PUBLIC LAGRANGE_WITH_ASSIMP)
endif()

# 3. installation
if(LAGRANGE_INSTALL)
    set_target_properties(lagrange_io PROPERTIES EXPORT_NAME io)
    lagrange_install(lagrange_io)
endif()

# 4. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()
