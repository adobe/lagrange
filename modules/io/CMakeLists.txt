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
set_target_properties(lagrange_io PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(LAGRANGE_TOPLEVEL_PROJECT)
    set_target_properties(lagrange_io PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

# 2. dependencies
lagrange_include_modules(core fs scene image_io)
include(tinyobjloader)
include(tinygltf)
include(libigl) # TODO: remove libigl later
include(mshio)
lagrange_find_package(happly REQUIRED)
include(ufbx)
target_link_libraries(lagrange_io
    PUBLIC
        lagrange::core
        lagrange::fs
        lagrange::scene
        tinyobjloader::tinyobjloader
        happly::happly
        igl::core
    PRIVATE
        lagrange::image_io
        tinygltf::tinygltf
        ufbx::ufbx
        mshio::mshio
)

option(LAGRANGE_WITH_ASSIMP "Add assimp functionality to lagrange::io" OFF)
if(LAGRANGE_WITH_ASSIMP)
    include(assimp)
    target_link_libraries(lagrange_io PUBLIC assimp::assimp)
    target_compile_definitions(lagrange_io PUBLIC LAGRANGE_WITH_ASSIMP)
endif()

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
