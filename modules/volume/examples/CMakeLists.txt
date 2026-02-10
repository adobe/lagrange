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
include(cli11)
lagrange_include_modules(io polyscope bvh)

lagrange_add_example(voxelize_cli voxelize_cli.cpp)
target_link_libraries(voxelize_cli
    lagrange::volume
    lagrange::io
    lagrange::bvh
    CLI11::CLI11
)
if(TARGET OpenVDB::nanovdb)
    target_link_libraries(voxelize_cli OpenVDB::nanovdb)
    target_compile_definitions(voxelize_cli PRIVATE NANOVDB_ENABLED)
endif()

lagrange_add_example(voxelize_gui voxelize_gui.cpp)
target_link_libraries(voxelize_gui
    lagrange::volume
    lagrange::polyscope
    lagrange::bvh
    lagrange::io
    CLI11::CLI11
)

lagrange_add_example(fill_with_spheres fill_with_spheres.cpp)
target_link_libraries(fill_with_spheres lagrange::volume lagrange::io CLI11::CLI11)

lagrange_add_example(grid_viewer grid_viewer.cpp)
target_link_libraries(grid_viewer lagrange::volume lagrange::io CLI11::CLI11 polyscope::polyscope)
if(TARGET OpenVDB::nanovdb)
    target_link_libraries(grid_viewer OpenVDB::nanovdb)
    target_compile_definitions(grid_viewer PRIVATE NANOVDB_ENABLED)
endif()
