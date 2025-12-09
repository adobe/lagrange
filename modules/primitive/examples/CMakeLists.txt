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
lagrange_include_modules(io curve_io)

lagrange_add_example(generate_octahedron generate_octahedron.cpp)
target_link_libraries(generate_octahedron lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_hexahedron generate_hexahedron.cpp)
target_link_libraries(generate_hexahedron lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_torus generate_torus.cpp)
target_link_libraries(generate_torus lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_rounded_cone generate_rounded_cone.cpp)
target_link_libraries(generate_rounded_cone lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_rounded_cube generate_rounded_cube.cpp)
target_link_libraries(generate_rounded_cube lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_rounded_plane generate_rounded_plane.cpp)
target_link_libraries(generate_rounded_plane lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_rounded_cylinder generate_rounded_cylinder.cpp)
target_link_libraries(generate_rounded_cylinder lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_sphere generate_sphere.cpp)
target_link_libraries(generate_sphere lagrange::primitive lagrange::io CLI11::CLI11)

lagrange_add_example(generate_swept_surface generate_swept_surface.cpp)
target_link_libraries(generate_swept_surface lagrange::primitive
    lagrange::curve_io lagrange::io CLI11::CLI11)
