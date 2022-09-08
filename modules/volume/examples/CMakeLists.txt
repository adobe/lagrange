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
lagrange_include_modules(io)

lagrange_add_example(voxelize_mesh voxelize_mesh.cpp)
target_link_libraries(voxelize_mesh lagrange::volume lagrange::io CLI11::CLI11)

lagrange_add_example(fill_with_spheres fill_with_spheres.cpp)
target_link_libraries(fill_with_spheres lagrange::volume lagrange::io CLI11::CLI11)
