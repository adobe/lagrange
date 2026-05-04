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
include(cli11)
lagrange_include_modules(io)

lagrange_add_example(weld_vertices weld_vertices.cpp)
target_link_libraries(weld_vertices lagrange::bvh lagrange::io CLI11::CLI11)

lagrange_add_example(remove_interior_shells remove_interior_shells.cpp)
target_link_libraries(remove_interior_shells lagrange::bvh lagrange::io CLI11::CLI11)

lagrange_include_modules(packing polyscope)
lagrange_add_example(uv_overlap uv_overlap.cpp WITH_UI)
target_link_libraries(uv_overlap lagrange::bvh lagrange::packing lagrange::polyscope lagrange::io CLI11::CLI11)
