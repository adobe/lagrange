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
lagrange_include_modules(io ui)

lagrange_add_performance(remove_duplicate_vertices remove_duplicate_vertices.cpp)
target_link_libraries(remove_duplicate_vertices lagrange::core lagrange::io)

lagrange_add_performance(predicates_speed predicates_speed.cpp)
target_link_libraries(predicates_speed lagrange::core)

lagrange_add_performance(refine_mesh refine.cpp)
target_link_libraries(refine_mesh lagrange::core lagrange::io)

lagrange_add_performance(remove_degenerate_triangles remove_degenerate_triangles.cpp)
target_link_libraries(remove_degenerate_triangles lagrange::core lagrange::io)

lagrange_add_performance(remove_short_edges remove_short_edges.cpp)
target_link_libraries(remove_short_edges lagrange::core lagrange::io)

lagrange_add_performance(mesh_initialization mesh_initialization.cpp)
target_link_libraries(mesh_initialization lagrange::core lagrange::io)

lagrange_add_performance(connectivity connectivity.cpp)
target_link_libraries(connectivity lagrange::core lagrange::io)

lagrange_add_performance(components components.cpp)
target_link_libraries(components lagrange::core lagrange::io)

lagrange_add_performance(dijkstra dijkstra.cpp)
target_link_libraries(dijkstra lagrange::core lagrange::io)

lagrange_add_performance(select_vertices_speed select_vertices_speed.cpp)
target_link_libraries(select_vertices_speed lagrange::core)

lagrange_add_performance(resolve_nonmanifoldness resolve_nonmanifoldness.cpp)
target_link_libraries(resolve_nonmanifoldness lagrange::core lagrange::io)

lagrange_add_performance(repeated_loading repeated_loading.cpp)
target_link_libraries(repeated_loading lagrange::core lagrange::io)

lagrange_add_performance(condense_uv attributes/condense_uv.cpp)
target_link_libraries(condense_uv lagrange::core lagrange::io)

lagrange_add_performance(marquee marquee.cpp)
target_link_libraries(marquee lagrange::core lagrange::io CLI11::CLI11)

lagrange_add_performance(array experimental/array.cpp)
target_link_libraries(array lagrange::core lagrange::io)

lagrange_add_performance(unify_index_buffer unify_index_buffer.cpp)
target_link_libraries(unify_index_buffer lagrange::core lagrange::io)

lagrange_add_performance(view_tangent_frame view_tangent_frame.cpp)
target_link_libraries(view_tangent_frame lagrange::core lagrange::io lagrange::ui CLI11::CLI11)
