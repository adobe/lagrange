#
# Copyright 2017 Adobe. All rights reserved.
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

lagrange_add_example(mesh_cleanup mesh_cleanup.cpp)
target_link_libraries(mesh_cleanup lagrange::core lagrange::io CLI11::CLI11)

lagrange_add_example(fix_nonmanifold fix_nonmanifold.cpp)
target_link_libraries(fix_nonmanifold lagrange::core lagrange::io CLI11::CLI11)

include(polyscope)
lagrange_add_example(mesh_viewer mesh_viewer.cpp)
target_link_libraries(mesh_viewer lagrange::core lagrange::io CLI11::CLI11 polyscope::polyscope)
