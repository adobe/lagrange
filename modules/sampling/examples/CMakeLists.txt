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
lagrange_include_modules(sampling io polyscope)
include(pcg)

lagrange_add_example(quad_sampler quad_sampler.cpp)
target_link_libraries(quad_sampler lagrange::sampling lagrange::io CLI11::CLI11 pcg_cpp::pcg_cpp)

lagrange_add_example(mesh_sampler_gui mesh_sampler_gui.cpp)
target_link_libraries(mesh_sampler_gui lagrange::sampling lagrange::io CLI11::CLI11 lagrange::polyscope)
