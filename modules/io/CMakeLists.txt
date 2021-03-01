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
add_library(lagrange_io INTERFACE)
add_library(lagrange::io ALIAS lagrange_io)
message(STATUS "Lagrange: creating target 'lagrange::io'")

include(GNUInstallDirs)
target_include_directories(lagrange_io INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 2. add a custom target to show headers in IDE
file(GLOB_RECURSE INC_FILES "include/*.h")
add_custom_target(lagrange_io_ SOURCES ${INC_FILES})

# 3. dependencies
lagrange_include_modules(core fs)
include(tinyobjloader)
include(libigl) # TODO: remove libigl later
target_link_libraries(lagrange_io INTERFACE
    lagrange::core
    lagrange::fs
    tinyobjloader::tinyobjloader
    igl::core
)

option(LAGRANGE_WITH_ASSIMP "Add assimp functionality to lagrange::io" FALSE)
if(LAGRANGE_WITH_ASSIMP)
    include(assimp)
    target_link_libraries(lagrange_io INTERFACE assimp::assimp)
    target_compile_definitions(lagrange_io INTERFACE LAGRANGE_WITH_ASSIMP)
endif()

# 4. installation
if(LAGRANGE_INSTALL)
    set_target_properties(lagrange_io PROPERTIES EXPORT_NAME io)
    lagrange_install(lagrange_io)
endif()

# 5. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()
