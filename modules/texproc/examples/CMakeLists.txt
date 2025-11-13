#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

lagrange_include_modules(io image_io)

lagrange_add_example(geodesic_dilation geodesic_dilation.cpp)
target_link_libraries(geodesic_dilation lagrange::texproc CLI11::CLI11 lagrange::io lagrange::image_io)

lagrange_add_example(texture_stitching texture_stitching.cpp)
target_link_libraries(texture_stitching lagrange::texproc CLI11::CLI11 lagrange::io lagrange::image_io)

lagrange_add_example(texture_filtering texture_filtering.cpp)
target_link_libraries(texture_filtering lagrange::texproc CLI11::CLI11 lagrange::io lagrange::image_io)

lagrange_add_example(texture_compositing texture_compositing.cpp)
target_link_libraries(texture_compositing lagrange::texproc CLI11::CLI11 lagrange::io lagrange::image_io)

lagrange_add_example(texture_rasterization texture_rasterization.cpp)
target_link_libraries(texture_rasterization lagrange::texproc CLI11::CLI11 lagrange::io lagrange::image_io)
