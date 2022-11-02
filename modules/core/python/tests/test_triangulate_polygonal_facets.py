#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import lagrange

from .assets import cube


class TestTriangulatePolygonalFacets:
    def test_empty_mesh(self):
        mesh = lagrange.SurfaceMesh()
        lagrange.triangulate_polygonal_facets(mesh)
        assert mesh.num_vertices == 0

    def test_cube(self, cube):
        mesh = cube

        # Update growth policy to allow copy.
        mesh.attribute(
            mesh.attr_id_corner_to_vertex
        ).growth_policy = lagrange.AttributeGrowthPolicy.WarnAndCopy

        lagrange.triangulate_polygonal_facets(mesh)
        assert mesh.num_facets == 12

    def test_cube_with_attribute(self, cube):
        mesh = cube
        attr_id = lagrange.compute_normal(mesh)

        # Update growth policy to allow copy.
        mesh.attribute(
            mesh.attr_id_corner_to_vertex
        ).growth_policy = lagrange.AttributeGrowthPolicy.WarnAndCopy

        lagrange.triangulate_polygonal_facets(mesh)
        assert mesh.num_facets == 12
        assert mesh.is_attribute_indexed(attr_id)

        normal_attr = mesh.indexed_attribute(attr_id)
        normal_indices = normal_attr.indices
        assert normal_indices.num_elements == mesh.num_corners
