#
# Copyright 2023 Adobe. All rights reserved.
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
include(nlohmann_json)
lagrange_include_modules(io)

lagrange_add_example(sample_points_in_mesh sample_points_in_mesh.cpp)
target_link_libraries(sample_points_in_mesh lagrange::winding lagrange::io CLI11::CLI11)

lagrange_add_example(fix_orientation fix_orientation.cpp)
target_link_libraries(fix_orientation lagrange::winding nlohmann_json::nlohmann_json lagrange::io CLI11::CLI11)
