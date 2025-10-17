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
import lagrange

import numpy as np

from .assets import cube_triangular


class TestComputeFacetCircumcenter:
    def check_centroid(self, mesh, centroid_attr_id):
        centroids = mesh.attribute(centroid_attr_id).data

        for i in range(mesh.num_facets):
            v0, v1, v2 = mesh.vertices[mesh.facets[i]]
            c = centroids[i]
            r0 = np.linalg.norm(c - v0)
            r1 = np.linalg.norm(c - v1)
            r2 = np.linalg.norm(c - v2)

            assert np.isclose(r0, r1)
            assert np.isclose(r1, r2)
            assert np.isclose(r0, r2)

    def test_cube(self, cube_triangular):
        mesh = cube_triangular
        attr_id = lagrange.compute_facet_circumcenter(mesh)
        self.check_centroid(mesh, attr_id)

    def test_square(self):
        mesh = lagrange.SurfaceMesh(2)
        mesh.add_vertex([0, 0])
        mesh.add_vertex([1, 0])
        mesh.add_vertex([1, 1])
        mesh.add_vertex([0, 10])
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(0, 2, 3)

        attr_id = lagrange.compute_facet_circumcenter(mesh)
        self.check_centroid(mesh, attr_id)
